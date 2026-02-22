/* effects.h -----------------------------------------------------------------
 * Custom audio effects chain.
 *
 * Place custom per-sample effects here (delay, chorus, reverb, distortion…).
 * Each effect is declared in this header and implemented in effects.c.
 *
 * Usage:
 *   Effects_Init();                   // call once before audio starts
 *   float out = Effects_Process(in);  // call every sample
 * ---------------------------------------------------------------------------*/
#ifndef EFFECTS_H
#define EFFECTS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Delay – PSRAM / SRAM backend selection
 *
 *   DELAY_USE_PSRAM  1  →  delay buffer lives in external PSRAM (no SRAM cost)
 *                           requires PSRAM_MemoryMapped_Init() before Effects_Init()
 *   DELAY_USE_PSRAM  0  →  delay buffer lives in internal SRAM
 * --------------------------------------------------------------------------*/
#define DELAY_USE_PSRAM   0             /* flip to 1 to use PSRAM            */

#define DLY_SAMPLES       22000         /* 500 ms @ 44 kHz                   */
#define DLY_FEEDBACK      0.60f
#define DLY_MIX           0.55f

/* ---------------------------------------------------------------------------
 * Chain enable flags
 * Set to 1 to include the effect in Effects_Process(), 0 to bypass it.
 * --------------------------------------------------------------------------*/
#define EFFECT_DELAY_ENABLE   0         /* delay is defined but bypassed     */

/* ---------------------------------------------------------------------------
 * Public API
 * --------------------------------------------------------------------------*/

/**
 * @brief  Initialise all effects (clears buffers, resets state).
 *         Call once before the audio render loop starts.
 */
void Effects_Init(void);

/**
 * @brief  Run one audio sample through the active effects chain.
 * @return Processed sample.
 */
float Effects_Process(float in);

#ifdef __cplusplus
}
#endif

#endif /* EFFECTS_H */
