/* sequencer.cpp -------------------------------------------------------------
 * Step sequencer using the Gingoduino music-theory engine.
 *
 * The sequence is built at runtime from GingoNote + GingoDuration objects.
 * Gingoduino handles pitch / MIDI / timing maths; we just post NoteEvents
 * to the synth core via the g_note_queue FreeRTOS message queue.
 * ---------------------------------------------------------------------------*/

/* Force Tier 3 before any Gingoduino header (gives Sequence + Event) */
#define GINGODUINO_TIER 3
/* Limit event capacity to what we actually use — shrinks GingoSequence from ~6KB to ~1.5KB */
#define GINGODUINO_MAX_EVENTS 16

#include "sequencer.h"

/* Gingoduino */
#include "Gingoduino.h"

/* C runtime */
#include <stddef.h>
#include <new>         /* placement new */

using namespace gingoduino;

/* ---------------------------------------------------------------------------
/* Static-storage Gingoduino objects — NOT on the task stack.
 * Initialized lazily inside Sequencer_Task to avoid static-init-order issues.
 * GingoSequence with 16 events ≈ 1.6 KB; safe in BSS/data. */
static uint8_t        s_seq_buf[sizeof(GingoSequence)];
static GingoSequence* s_seq = nullptr;

/* ---------------------------------------------------------------------------
 * Sequence definition
 *
 * Using GingoEvent::noteEvent / rest factory helpers.
 * GingoTempo(130) → 130 BPM → quarter = 461 ms
 * --------------------------------------------------------------------------*/
static void build_sequence(GingoSequence& seq)
{
    const GingoDuration quarter("quarter");
    const GingoDuration eighth("eighth");

    /* A small melodic motif in D minor */
    seq.add(GingoEvent::noteEvent(GingoNote("D"),  quarter, 4, 80));
    seq.add(GingoEvent::noteEvent(GingoNote("G"),  quarter, 3, 80));
    seq.add(GingoEvent::noteEvent(GingoNote("A"),  quarter, 3, 80));
    seq.add(GingoEvent::noteEvent(GingoNote("B"),  quarter, 3, 80));
    seq.add(GingoEvent::rest(quarter));
    seq.add(GingoEvent::noteEvent(GingoNote("A"),  quarter, 3, 80));
    seq.add(GingoEvent::rest(eighth));
    seq.add(GingoEvent::noteEvent(GingoNote("F"),  quarter, 4, 80));
    seq.add(GingoEvent::noteEvent(GingoNote("D"),  quarter, 4, 80));
    seq.add(GingoEvent::rest(quarter));
}

/* ---------------------------------------------------------------------------
 * Sequencer_Init  (called before osKernelStart — nothing to do here)
 * --------------------------------------------------------------------------*/
void Sequencer_Init(void)
{
    /* Sequence is built inline in Sequencer_Task; nothing to pre-init. */
}

/* ---------------------------------------------------------------------------
 * Sequencer_Task
 * --------------------------------------------------------------------------*/
void Sequencer_Task(void *argument)
{
    (void)argument;

    /* Wait until Synth_Task has created the queue */
    while (g_note_queue == NULL)
        osDelay(1);

    /* Construct GingoSequence in static storage (placement new avoids heap
     * and sidesteps the static-init-order problem entirely) */
    GingoTempo   tempo(130.0f);
    GingoTimeSig timeSig(4, 4);
    s_seq = new (s_seq_buf) GingoSequence(tempo, timeSig);
    build_sequence(*s_seq);

    const float ms_per_beat = tempo.msPerBeat();

    for (;;)
    {
        for (uint8_t i = 0; i < s_seq->size(); i++)
        {
            const GingoEvent& evt = s_seq->at(i);
            const float step_ms  = evt.duration().beats() * ms_per_beat;
            const float gate_ms  = (evt.type() == EVENT_REST) ? 0.0f
                                   : step_ms * 0.80f;   /* 80 % gate */

            /* --- Note ON -------------------------------------------------- */
            if (evt.type() == EVENT_NOTE)
            {
                NoteEvent on_evt;
                on_evt.note      = evt.midiNumber();
                on_evt.velocity  = evt.velocity();
                on_evt.frequency = evt.frequency();
                osMessageQueuePut(g_note_queue, &on_evt, 0U, 0U);
            }

            /* Hold for gate time */
            if (gate_ms > 0.0f)
                osDelay((uint32_t)gate_ms);

            /* --- Note OFF ------------------------------------------------- */
            {
                NoteEvent off_evt = { 0, 0, 0.0f };
                osMessageQueuePut(g_note_queue, &off_evt, 0U, 0U);
            }

            /* Gap: remainder of step */
            const uint32_t gap_ms = (uint32_t)(step_ms - gate_ms);
            if (gap_ms > 0)
                osDelay(gap_ms);
        }
    }
}
