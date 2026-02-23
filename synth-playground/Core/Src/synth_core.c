#include "synth_core.h"
#include "sequencer.h"
#include "effects.h"
#include "leaf.h"
#include "leaf-envelopes.h"
#include "utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "cmsis_os2.h"

/* ---------------------------------------------------------------------------
 * Constants
 * --------------------------------------------------------------------------*/
#define SAMPLERATE       44000
#define LEAF_MEMPOOL_SIZE 30000
#define DMA_BUFFER_SIZE   8192   /* Total DMA buffer size (must be even) */

/* ---------------------------------------------------------------------------
 * LEAF
 * --------------------------------------------------------------------------*/
static char mempool[LEAF_MEMPOOL_SIZE];
LEAF leaf;

static tPBSaw        *osc;
static tTriLFO *lfo;
static tSVF          *filter;
static tHybridADSR   *env;

static float rnd_func(void)
{
    return ((float)rand() / (float)RAND_MAX);
}

/* ---------------------------------------------------------------------------
 * DMA double-buffer
 * --------------------------------------------------------------------------*/
static uint16_t dma_buffer[DMA_BUFFER_SIZE];

volatile uint8_t  need_samples          = 0;
volatile uint32_t buffer_overrun_count  = 0;
volatile uint32_t half_complete_count   = 0;
volatile uint32_t full_complete_count   = 0;
volatile uint32_t buffers_filled_count  = 0;

/* Queue shared with the sequencer */
osMessageQueueId_t g_note_queue = NULL;

extern I2S_HandleTypeDef hi2s1;

/* ---------------------------------------------------------------------------
 * DMA callbacks
 * --------------------------------------------------------------------------*/
void I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
    half_complete_count++;
    if (need_samples == 0)
        need_samples = 1;   /* fill first half */
    else
        buffer_overrun_count++;
}

void I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
    full_complete_count++;
    if (need_samples == 0)
        need_samples = 2;   /* fill second half */
    else
        buffer_overrun_count++;
}

/* ---------------------------------------------------------------------------
 * Synth_Init
 * --------------------------------------------------------------------------*/
void Synth_Init(void)
{
    LEAF_init(&leaf, SAMPLERATE, mempool, LEAF_MEMPOOL_SIZE, &rnd_func);

    tPBSaw_init(&osc, &leaf);
    tPBSaw_setFreq(osc, 440.0f);

    tTriLFO_init(&lfo, &leaf);
    tTriLFO_setFreq(lfo, 0.5f);

    tSVF_init(&filter, SVFTypeLowpass, 400.0f, 3.0f, &leaf);

    /* HybridADSR: linear attack, exponential decay/release, no lookup tables.
     * Args: attack ms, decay ms, sustain level, release ms */
    tHybridADSR_init(&env, 5.0f, 80.0f, 0.7f, 150.0f, &leaf);

    Effects_Init();

    HAL_GPIO_WritePin(AUDIO_MUTE_CONTROL_GPIO_Port, AUDIO_MUTE_CONTROL_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    HAL_I2S_RegisterCallback(&hi2s1, HAL_I2S_TX_COMPLETE_CB_ID,      I2S_TxCpltCallback);
    HAL_I2S_RegisterCallback(&hi2s1, HAL_I2S_TX_HALF_COMPLETE_CB_ID, I2S_TxHalfCpltCallback);

    /* Pre-fill buffer with silence */
    memset(dma_buffer, 0, sizeof(dma_buffer));

    if (HAL_I2S_Transmit_DMA(&hi2s1, dma_buffer, DMA_BUFFER_SIZE) != HAL_OK)
        Error_Handler();
}

/* ---------------------------------------------------------------------------
 * Synth_Task
 * --------------------------------------------------------------------------*/
void Synth_Task(void *argument)
{
    g_note_queue = osMessageQueueNew(8, sizeof(NoteEvent), NULL);

    while (1)
    {
        /* --- Consume ONE pending NoteEvent per loop iteration ------------ */
        NoteEvent evt;
        if (osMessageQueueGet(g_note_queue, &evt, NULL, 0U) == osOK)
        {
            if (evt.velocity > 0 && evt.frequency > 0.0f)
            {
                tPBSaw_setFreq(osc, evt.frequency);
                tHybridADSR_on(env, evt.velocity / 127.0f);
            }
            else
            {
                tHybridADSR_off(env);
            }
        }

        /* --- Fill DMA buffer half when requested -------------------------- */
        if (need_samples != 0)
        {
            uint8_t  half      = need_samples;
            need_samples       = 0;
            uint32_t num_frames = DMA_BUFFER_SIZE / 4;  /* stereo frames per half */
            uint16_t *write_ptr = (half == 1) ? dma_buffer
                                               : dma_buffer + DMA_BUFFER_SIZE / 2;

            for (uint32_t f = 0; f < num_frames; f++)
            {
                /* HybridADSR envelope output */
                float env_out = tHybridADSR_tick(env);

                /* LFO → filter cutoff sweep (100–8000 Hz) */
                float lfo_val = tTriLFO_tick(lfo);
                float cutoff  = 4050.0f + lfo_val * 3950.0f;
                tSVF_setFreq(filter, cutoff);

                /* Oscillator → envelope → filter → effects */
                float osc_out  = tPBSaw_tick(osc) * env_out;
                float filtered = tSVF_tickLP(filter, osc_out);
                float sample   = Effects_Process(filtered);

                int16_t out = (int16_t)(sample * 32767.0f * 0.10f);
                write_ptr[f * 2 + 0] = (uint16_t)out;  /* L */
                write_ptr[f * 2 + 1] = (uint16_t)out;  /* R */
            }

            buffers_filled_count++;
        }

        /* Yield so equal-priority tasks (sequencer) get CPU time.
         * osDelay(1) blocks for one tick (1ms) — audio half-buffer is ~46ms
         * at 44kHz / 8192 samples, so this adds no audible latency. */
        osDelay(1);
    }
}
