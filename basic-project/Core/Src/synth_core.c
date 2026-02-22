#include "synth_core.h"
#include "sequencer.h"
#include "leaf.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "cmsis_os2.h"

// ---------------------------------------------------------------------------
// Circular delay line — select backend with DELAY_USE_PSRAM:
//   0 = SRAM  (safe, always works, 88 KB of internal SRAM)
//   1 = PSRAM (0 SRAM cost, needs PSRAM_MemoryMapped_Init() before Synth_Init())
// ---------------------------------------------------------------------------
#define DELAY_USE_PSRAM  1            // <-- flip to 0 to fall back to SRAM

#define DLY_SAMPLES   22000           // 500 ms @ 44 kHz
#define DLY_FEEDBACK  0.60f
#define DLY_MIX       0.55f

#if DELAY_USE_PSRAM
  // PSRAM memory-mapped base — writable after PSRAM_MemoryMapped_Init()
  static float * const dly_buf   = (float *)OCTOSPI1_MEM_BASE;
#else
  static float         dly_buf[DLY_SAMPLES];   // 88 KB in SRAM
#endif

static uint32_t dly_write = 0;

// Clear the delay buffer (works for both SRAM and PSRAM)
static void delay_clear(void)
{
    // Write zeros one word at a time — safe for both SRAM and PSRAM
    for (uint32_t i = 0; i < DLY_SAMPLES; i++)
        dly_buf[i] = 0.0f;
    dly_write = 0;
}

static inline float delay_process(float in)
{
    // dly_write always points at the OLDEST slot in the ring buffer
    float out = dly_buf[dly_write];
    dly_buf[dly_write] = in + out * DLY_FEEDBACK;
    if (++dly_write >= DLY_SAMPLES)
        dly_write = 0;
    return out;
}

/* Queue shared with the sequencer – holds up to 8 pending NoteEvents */
osMessageQueueId_t g_note_queue = NULL;

// External handles from main.c
extern I2S_HandleTypeDef hi2s1;

// LEAF Constants
#define SAMPLERATE 44000
#define LEAF_BUFFER_SIZE (2 * 44000)
#define DMA_BUFFER_SIZE 8192  // Total DMA buffer size (must be even)

// LEAF Memory pool
char mempool[30000];

// Double buffer for DMA - stereo 16-bit samples
uint16_t dma_buffer[DMA_BUFFER_SIZE] = {0};

// Buffer management for double-buffering
volatile uint16_t *current_write_ptr = dma_buffer;
volatile size_t current_sample_count = 0;
volatile uint32_t buffer_overrun_count = 0;
volatile uint8_t need_samples = 0;  // Flag: 0=none, 1=fill first half, 2=fill second half
volatile uint32_t half_complete_count = 0;  // Increments on half-complete callback
volatile uint32_t full_complete_count = 0;  // Increments on full-complete callback
volatile uint32_t buffers_filled_count = 0; // Increments each time we fill a buffer half

// LEAF objects
LEAF leaf;
tPBTriangle* osc;
tTriLFO*     lfo;
tSVF*        filter;
// tDelay*   dly;  -- replaced by PSRAM delay line

// Gate envelope smoother – avoids clicks on note on/off
// synth_gate_target: 1.0 = on, 0.0 = off  (written by NoteEvent handler)
// synth_gate_smooth: actual per-sample amplitude  (updated inside render loop)
// GATE_COEFF: one-pole lowpass coefficient – ~3 ms at 44 kHz
//   coeff = 1 - exp(-1 / (tau_samples))  where tau = 0.003 * 44000 = 132
#define GATE_COEFF 0.0075f
static volatile float synth_gate_target = 0.0f;
static float          synth_gate_smooth  = 0.0f;

// Random number generator for LEAF
static float rnd_func()
{
    return ((float)rand() / (float)(RAND_MAX));
}

// DMA Half Transfer Complete Callback - first half sent, fill it
void I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  half_complete_count++;  // Debug counter
  // DMA is now playing second half, we need to fill first half
  if (need_samples == 0)
  {
    need_samples = 1;  // Request first half fill
  }
  else
  {
    buffer_overrun_count++;  // Previous fill not complete
  }
}

// DMA Transfer Complete Callback - second half sent, fill it
void I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  full_complete_count++;  // Debug counter
  // DMA is now playing first half, we need to fill second half
  if (need_samples == 0)
  {
    need_samples = 2;  // Request second half fill
  }
  else
  {
    buffer_overrun_count++;  // Previous fill not complete
  }
}

void Synth_Init(void)
{
  // Initialize LEAF audio library
  LEAF_init(&leaf, SAMPLERATE, mempool, LEAF_BUFFER_SIZE, &rnd_func);
  
  // Initialize a polyblep triangle oscillator
  tPBTriangle_init(&osc, &leaf);
  tPBTriangle_setFreq(osc, 440.0f);   // default pitch, overridden by sequencer
  synth_gate_target = 0.0f;           // silent until sequencer sends a note-on
  synth_gate_smooth  = 0.0f;

  // Triangle LFO – 0.5 Hz → 2-second full sweep
  tTriLFO_init(&lfo, &leaf);
  tTriLFO_setFreq(lfo, 0.5f);

  // State-variable lowpass filter – Q=3.0 gives a strong resonant peak
  tSVF_init(&filter, SVFTypeLowpass, 400.0f, 3.0f, &leaf);

  // Clear delay buffer
  delay_clear();

  // Unmute PCM5102A
  HAL_GPIO_WritePin(AUDIO_MUTE_CONTROL_GPIO_Port, AUDIO_MUTE_CONTROL_Pin, GPIO_PIN_SET);
  HAL_Delay(10); // Use HAL_Delay since scheduler isn't running yet
  
  // Register both I2S callbacks for double-buffering
  HAL_I2S_RegisterCallback(&hi2s1, HAL_I2S_TX_COMPLETE_CB_ID, I2S_TxCpltCallback);
  HAL_I2S_RegisterCallback(&hi2s1, HAL_I2S_TX_HALF_COMPLETE_CB_ID, I2S_TxHalfCpltCallback);

  // Pre-fill buffer with silence before starting DMA
  for (uint32_t i = 0; i < DMA_BUFFER_SIZE / 2; i++)
  {
    tPBTriangle_tick(osc);  // advance oscillator phase, discard output
    dma_buffer[i * 2]     = 0;
    dma_buffer[i * 2 + 1] = 0;
  }
  
  // Start I2S DMA transmission with pre-filled buffer
  HAL_StatusTypeDef i2s_status = HAL_I2S_Transmit_DMA(&hi2s1, dma_buffer, DMA_BUFFER_SIZE);
  if (i2s_status != HAL_OK) {
    Error_Handler();
  }
}

void Synth_Task(void *argument)
{
  // Create the note queue here – we are now running under the scheduler
  g_note_queue = osMessageQueueNew(8, sizeof(NoteEvent), NULL);

  while (1)
  {
    // --- Consume any pending NoteEvents from the sequencer ---------------
    NoteEvent evt;
    while (osMessageQueueGet(g_note_queue, &evt, NULL, 0U) == osOK)
    {
      if (evt.velocity > 0 && evt.frequency > 0.0f)
      {
        // Note ON: set pitch and open gate
        tPBTriangle_setFreq(osc, evt.frequency);
        synth_gate_target = 1.0f;
      }
      else
      {
        // Note OFF: close gate smoothly
        synth_gate_target = 0.0f;
      }
    }

    // Check if a buffer half needs filling
    if (need_samples != 0)
    {
      uint8_t buffer_to_fill = need_samples;
      need_samples = 0;  // Clear flag immediately
      
      // Set write pointer to appropriate buffer half
      uint16_t* write_ptr;
      if (buffer_to_fill == 1)
      {
        write_ptr = dma_buffer;  // Fill first half
      }
      else
      {
        write_ptr = dma_buffer + DMA_BUFFER_SIZE / 2;  // Fill second half
      }
      
      // Generate samples for this half
      // Each half = DMA_BUFFER_SIZE/2 = 4096 uint16_t values = 2048 stereo frames
      uint32_t num_frames = DMA_BUFFER_SIZE / 4;  // 2048 stereo frames per half
      
      for (uint32_t frame = 0; frame < num_frames; frame++)
      {
        // Smooth the gate toward its target (eliminates clicks)
        synth_gate_smooth += GATE_COEFF * (synth_gate_target - synth_gate_smooth);

        // LFO sweeps filter cutoff between 100 Hz and 8000 Hz
        float lfo_val = tTriLFO_tick(lfo);                   // -1 .. +1
        float cutoff  = 4050.0f + lfo_val * 3950.0f;        // 100 .. 8000 Hz
        tSVF_setFreq(filter, cutoff);                        // takes Hz directly

        // Gate only the oscillator; delay tail must ring freely after note-off
        float osc_out  = tPBTriangle_tick(osc) * synth_gate_smooth;
        float filtered = tSVF_tickLP(filter, osc_out);

        // Delay: dry goes in gated, echo output is always mixed regardless of gate
        float echo   = delay_process(filtered);
        float sample = filtered + echo * DLY_MIX;
        
        // Convert to 16-bit signed integer
        int16_t sample_int = (int16_t)(sample * 32767.0f * 0.25f);
        
        // Write stereo frame (duplicate mono to L/R)
        write_ptr[frame * 2 + 0] = (uint16_t)sample_int;  // Left
        write_ptr[frame * 2 + 1] = (uint16_t)sample_int;  // Right
      }
      
      buffers_filled_count++;  // Debug counter - increment after filling
    }
    else
    {
      // Yield to other tasks if no audio processing is needed right now
      osDelay(1);
    }
  }
}
