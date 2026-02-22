/* sequencer.c ---------------------------------------------------------------
 * Step sequencer module.
 *
 * Sequencer_Task walks through a SequenceStep array at runtime speed
 * and posts NoteEvent messages to synth_core via g_note_queue.
 *
 * Timing is driven entirely by osDelay(), so the scheduler tick resolution
 * (1 ms by default) is the smallest usable step duration.
 * ---------------------------------------------------------------------------*/
#include "sequencer.h"
#include <math.h>    /* powf */
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * Internal state
 * --------------------------------------------------------------------------*/
static const SequenceStep *s_steps   = NULL;
static uint32_t            s_length  = 0;
static uint8_t             s_loop    = 1;

/* ---------------------------------------------------------------------------
 * Sequencer_Init
 * --------------------------------------------------------------------------*/
void Sequencer_Init(const SequenceStep *steps, uint32_t length, uint8_t loop)
{
    s_steps  = steps;
    s_length = length;
    s_loop   = loop;
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

    if (s_steps == NULL || s_length == 0)
    {
        /* Nothing to play – park the task */
        osThreadSuspend(osThreadGetId());
        for (;;) { osDelay(1000); }
    }

    do
    {
        for (uint32_t i = 0; i < s_length; i++)
        {
            const SequenceStep *step = &s_steps[i];

            /* --- Note ON -------------------------------------------------- */
            if (step->velocity > 0 && step->note > 0)
            {
                NoteEvent on_evt = {
                    .note      = step->note,
                    .velocity  = step->velocity,
                    .frequency = MIDI_NOTE_TO_HZ(step->note)
                };
                /* Non-blocking put; if queue full, the synth can't keep up */
                osMessageQueuePut(g_note_queue, &on_evt, 0U, 0U);
            }

            /* Gate time – note is held */
            uint32_t gate = step->gate_ms;
            if (gate > step->step_ms) gate = step->step_ms;
            if (gate > 0)
            {
                osDelay(gate);
            }

            /* --- Note OFF ------------------------------------------------- */
            {
                NoteEvent off_evt = {
                    .note      = 0,
                    .velocity  = 0,
                    .frequency = 0.0f
                };
                osMessageQueuePut(g_note_queue, &off_evt, 0U, 0U);
            }

            /* Gap between gate end and next step */
            uint32_t gap = step->step_ms - gate;
            if (gap > 0)
            {
                osDelay(gap);
            }
        }
    }
    while (s_loop);

    /* One-shot: park the task after the sequence finishes */
    osThreadSuspend(osThreadGetId());
    for (;;) { osDelay(1000); }
}
