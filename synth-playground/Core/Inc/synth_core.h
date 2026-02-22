#ifndef SYNTH_CORE_H
#define SYNTH_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "cmsis_os2.h"
#include "sequencer.h"   /* NoteEvent */

/**
 * @brief FreeRTOS message queue used to pass NoteEvent from the sequencer
 *        to the synth core.  Created in Synth_Init().
 */
extern osMessageQueueId_t g_note_queue;

/**
 * @brief Initialize the synthesizer (LEAF, oscillators, DMA, queue).
 *        Call this before starting the FreeRTOS scheduler.
 */
void Synth_Init(void);

/**
 * @brief The main synthesizer task loop.
 *        Handles audio buffer generation and consumes NoteEvents.
 */
void Synth_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif // SYNTH_CORE_H
