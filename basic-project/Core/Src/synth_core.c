#include "synth_core.h"
#include "sequencer.h"
#include "leaf.h"
#include <math.h>
#include <stdlib.h>
#include "cmsis_os2.h"

/* Queue shared with the sequencer – holds up to 8 pending NoteEvents */
osMessageQueueId_t g_note_queue = NULL;

// External handles from main.c
extern I2S_HandleTypeDef hi2s1;

// LEAF Constants
#define SAMPLERATE 44000
#define LEAF_BUFFER_SIZE (2 * 44000)
#define DMA_BUFFER_SIZE 8192  // Total DMA buffer size (must be even)

// LEAF Memory pool
char mempool[10000];

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
tCycle* cycle;
tHermiteDelay* delay;

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
  
  // Initialize a sine oscillator (tCycle)
  tCycle_init(&cycle, &leaf);
  tCycle_setFreq(cycle, 440.0f);  // will be overridden by first NoteEvent
  synth_gate_target = 0.0f;       // silent until sequencer sends a note-on
  synth_gate_smooth  = 0.0f;

  // Unmute PCM5102A
  HAL_GPIO_WritePin(AUDIO_MUTE_CONTROL_GPIO_Port, AUDIO_MUTE_CONTROL_Pin, GPIO_PIN_SET);
  HAL_Delay(10); // Use HAL_Delay since scheduler isn't running yet
  
  // Register both I2S callbacks for double-buffering
  HAL_I2S_RegisterCallback(&hi2s1, HAL_I2S_TX_COMPLETE_CB_ID, I2S_TxCpltCallback);
  HAL_I2S_RegisterCallback(&hi2s1, HAL_I2S_TX_HALF_COMPLETE_CB_ID, I2S_TxHalfCpltCallback);

  // Pre-fill buffer with silence before starting DMA
  for (uint32_t i = 0; i < DMA_BUFFER_SIZE / 2; i++)
  {
    tCycle_tick(cycle);  // advance oscillator phase, discard output
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
        // Note ON: retune the oscillator and open gate
        tCycle_setFreq(cycle, evt.frequency);
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

        // Generate one mono sample from LEAF oscillator, gated
        float sample = tCycle_tick(cycle) * synth_gate_smooth;
        
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
