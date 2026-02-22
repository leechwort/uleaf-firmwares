/* sequencer.h ---------------------------------------------------------------
 * Step sequencer module.
 *
 * Defines a fixed note sequence and posts NoteEvent messages to synth_core
 * via a FreeRTOS message queue.  The sequencer runs in its own RTOS task.
 * ---------------------------------------------------------------------------*/
#ifndef SEQUENCER_H
#define SEQUENCER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <math.h>    /* powf – required by MIDI_NOTE_TO_HZ */
#include "cmsis_os2.h"

/* ---------------------------------------------------------------------------
 * Note / pitch helpers
 * MIDI note 69 = A4 = 440 Hz
 * f = 440 * 2^((note - 69) / 12)
 * --------------------------------------------------------------------------*/
#define MIDI_NOTE_TO_HZ(n) (440.0f * powf(2.0f, ((float)(n) - 69.0f) / 12.0f))

/* ---------------------------------------------------------------------------
 * NoteEvent – sent from sequencer to synth core
 * --------------------------------------------------------------------------*/
typedef struct {
    uint8_t  note;        /* MIDI note number  (0 = rest / note-off) */
    uint8_t  velocity;    /* 0 = note-off, 1-127 = note-on            */
    float    frequency;   /* Hz, pre-computed from note               */
} NoteEvent;

/* ---------------------------------------------------------------------------
 * SequenceStep – one step in the sequence definition
 * --------------------------------------------------------------------------*/
typedef struct {
    uint8_t  note;            /* MIDI note number; 0 = rest            */
    uint8_t  velocity;        /* 0 = rest / silent                     */
    uint32_t gate_ms;         /* how long the note is on  (ms)         */
    uint32_t step_ms;         /* total step duration (gate + gap)  (ms)*/
} SequenceStep;

/* ---------------------------------------------------------------------------
 * Queue handle – created by Synth_Init, read by Synth_Task,
 *               written by Sequencer_Task.
 * --------------------------------------------------------------------------*/
extern osMessageQueueId_t g_note_queue;

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------*/

/**
 * @brief  Initialise the sequencer (call before osKernelStart).
 *         Stores the sequence to play; does NOT create the task.
 *
 * @param  steps    Pointer to an array of SequenceStep.
 * @param  length   Number of steps in the array.
 * @param  loop     Non-zero to loop endlessly, 0 to play once.
 */
void Sequencer_Init(const SequenceStep *steps, uint32_t length, uint8_t loop);

/**
 * @brief  FreeRTOS task function for the sequencer.
 *         Pass to osThreadNew().
 */
void Sequencer_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* SEQUENCER_H */
