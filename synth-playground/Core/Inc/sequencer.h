/* sequencer.h ---------------------------------------------------------------
 * Step sequencer module.
 *
 * The note sequence is defined internally in sequencer.cpp using Gingoduino.
 * Posts NoteEvent messages to synth_core via a FreeRTOS message queue.
 * The sequencer runs in its own RTOS task.
 * ---------------------------------------------------------------------------*/
#ifndef SEQUENCER_H
#define SEQUENCER_H

#include <stdint.h>
#include "cmsis_os2.h"

/* ---------------------------------------------------------------------------
 * NoteEvent – sent from sequencer to synth core.
 * Plain C struct so synth_core.c (compiled as C) can use it directly.
 * --------------------------------------------------------------------------*/
typedef struct {
    uint8_t  note;        /* MIDI note number  (0 = rest / note-off) */
    uint8_t  velocity;    /* 0 = note-off, 1-127 = note-on            */
    float    frequency;   /* Hz, computed by Gingoduino               */
} NoteEvent;

/* ---------------------------------------------------------------------------
 * Queue handle – created by Synth_Task(), written by Sequencer_Task().
 * --------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

extern osMessageQueueId_t g_note_queue;

void Sequencer_Init(void);
void Sequencer_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* SEQUENCER_H */
