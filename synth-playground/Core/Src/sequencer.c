/* sequencer.c ---------------------------------------------------------------
 * Step sequencer module.
 *
 * The note sequence is defined here.  Sequencer_Task walks through the steps
 * and posts NoteEvent messages to synth_core via g_note_queue.
 *
 * Timing is driven entirely by osDelay(), so the scheduler tick resolution
 * (1 ms by default) is the smallest usable step duration.
 * ---------------------------------------------------------------------------*/
#include "sequencer.h"
#include <math.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Note sequence
 * Edit this table to change what the synth plays.
 * note=0 or velocity=0 → rest.
 * --------------------------------------------------------------------------*/
static const SequenceStep s_sequence[] = {
    /* note  vel  gate_ms  step_ms */
    {  62,   80,   180,     230  },   /* D4  ~293.7 Hz */
    {  55,   80,   180,     230  },   /* G3  ~196.0 Hz */
    {  57,   80,   180,     230  },   /* A3  ~220.0 Hz */
    {  59,   80,   180,     230  },   /* B3  ~246.9 Hz */
    {   0,    0,     0,     230  },   /* rest           */
    {  57,   80,   180,     230  },   /* A3  ~220.0 Hz */
    {   0,    0,     0,     230  },   /* rest           */
    {  65,   80,   180,     230  },   /* F4  ~349.2 Hz */
    {  62,   80,   180,     230  },   /* D4  ~293.7 Hz */
    {   0,    0,     0,     230  },   /* rest           */
};

#define SEQUENCE_LENGTH (sizeof(s_sequence) / sizeof(s_sequence[0]))

/* ---------------------------------------------------------------------------
 * Sequencer_Init
 * --------------------------------------------------------------------------*/
void Sequencer_Init(void)
{
    /* Nothing to initialise – sequence is statically defined above. */
}

/* ---------------------------------------------------------------------------
 * Sequencer_Task
 * --------------------------------------------------------------------------*/
void Sequencer_Task(void *argument)
{
    (void)argument;

    /* Wait until Synth_Task has created the queue */
    while (g_note_queue == NULL)
    {
        osDelay(1);
    }

    for (;;)
    {
        for (uint32_t i = 0; i < SEQUENCE_LENGTH; i++)
        {
            const SequenceStep *step = &s_sequence[i];

            /* --- Note ON -------------------------------------------------- */
            if (step->velocity > 0 && step->note > 0)
            {
                NoteEvent on_evt = {
                    .note      = step->note,
                    .velocity  = step->velocity,
                    .frequency = MIDI_NOTE_TO_HZ(step->note)
                };
                osMessageQueuePut(g_note_queue, &on_evt, 0U, 0U);
            }

            /* Gate time – note is held */
            uint32_t gate = (step->gate_ms < step->step_ms) ? step->gate_ms : step->step_ms;
            if (gate > 0)
                osDelay(gate);

            /* --- Note OFF ------------------------------------------------- */
            {
                NoteEvent off_evt = { .note = 0, .velocity = 0, .frequency = 0.0f };
                osMessageQueuePut(g_note_queue, &off_evt, 0U, 0U);
            }

            /* Gap between gate end and next step */
            uint32_t gap = step->step_ms - gate;
            if (gap > 0)
                osDelay(gap);
        }
    }
}
