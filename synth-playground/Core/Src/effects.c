/* effects.c -----------------------------------------------------------------
 * Custom audio effects chain.
 *
 * Add new effects below following the same pattern as Delay:
 *   – declare state at file scope
 *   – write an _init() and a _process() function
 *   – call _init() from Effects_Init()
 *   – add an EFFECT_xxx_ENABLE guard in Effects_Process()
 * ---------------------------------------------------------------------------*/
#include "effects.h"
#include "main.h"   /* OCTOSPI1_MEM_BASE when DELAY_USE_PSRAM=1 */
#include <string.h>

/* ===========================================================================
 * Delay
 *
 * Buffer backend is selected by DELAY_USE_PSRAM in effects.h:
 *   0 → internal SRAM  (DLY_SAMPLES * 4 bytes, e.g. 88 KB for 22 000 samples)
 *   1 → external PSRAM memory-mapped via OctoSPI
 *       requires PSRAM_MemoryMapped_Init() to be called before Effects_Init()
 * ===========================================================================*/

#if DELAY_USE_PSRAM
  static float * const s_dly_buf = (float *)OCTOSPI1_MEM_BASE;
#else
  static float s_dly_buf[DLY_SAMPLES];
#endif

static uint32_t s_dly_write = 0;

static void delay_init(void)
{
    for (uint32_t i = 0; i < DLY_SAMPLES; i++)
        s_dly_buf[i] = 0.0f;
    s_dly_write = 0;
}

static float delay_process(float in)
{
    float out = s_dly_buf[s_dly_write];
    s_dly_buf[s_dly_write] = in + out * DLY_FEEDBACK;
    if (++s_dly_write >= DLY_SAMPLES)
        s_dly_write = 0;
    return out;
}

/* ===========================================================================
 * Add more effects here – follow the same pattern.
 * ===========================================================================*/

/* ===========================================================================
 * Public API
 * ===========================================================================*/

void Effects_Init(void)
{
    delay_init();
    /* call init for any additional effects here */
}

float Effects_Process(float in)
{
    float out = in;

#if EFFECT_DELAY_ENABLE
    float echo = delay_process(out);
    out = out + echo * DLY_MIX;
#endif

    /* add further effects here, e.g.:
     * #if EFFECT_CHORUS_ENABLE
     *     out = chorus_process(out);
     * #endif
     */

    return out;
}
