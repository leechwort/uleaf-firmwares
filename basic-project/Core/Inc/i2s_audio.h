/**
 ******************************************************************************
 * @file           : i2s_audio.h
 * @brief          : I2S audio utility functions for PCM5102A debugging
 ******************************************************************************
 */

#ifndef __I2S_AUDIO_H
#define __I2S_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h5xx_hal.h"
#include <stdint.h>

/* PCM5102A DAC Configuration Notes:
 * 
 * PCM5102A Hardware Pins (your setup):
 * - BCK  (Bit Clock)    <- I2S1_CK  (PA5)
 * - DIN  (Data In)      <- I2S1_SD  (PA7)
 * - LRCK (LR Clock/WS)  <- I2S2_WS  (PB12) ** CHECK THIS! Should be I2S1_WS! **
 * - XSMT (Mute Control) <- PC15 (AUDIO_MUTE_CONTROL)
 * 
 * PCM5102A pins tied to GND:
 * - SCK  (System Clock) - GND = Use BCK as system clock (no MCLK needed)
 * - FLT  (Filter)       - GND = Normal latency filter
 * - DEMP (De-emphasis)  - GND = De-emphasis off
 * - FMT  (Format)       - GND = I2S format (default)
 * 
 * PCM5102A pins floating/VCC:
 * - VCC  = 3.3V or 5V
 * - GND  = Ground
 * - AGND = Analog ground
 * - AVCC = Analog VCC
 * - CPGND = Charge pump ground
 * - CPVCC = Charge pump VCC
 * 
 * IMPORTANT: XSMT (mute) pin:
 * - LOW  = Mute enabled (no audio output)
 * - HIGH = Mute disabled (audio plays)
 */

/* Audio test signals */
extern const uint16_t sine_wave_440hz_48k[];
extern const uint16_t triangle_wave[];
extern const uint32_t sine_wave_440hz_48k_size;
extern const uint32_t triangle_wave_size;

/* Function prototypes */

/**
 * @brief Initialize PCM5102A DAC (unmute control pin)
 * @param hi2s Pointer to I2S handle
 * @retval HAL status
 */
HAL_StatusTypeDef PCM5102A_Init(I2S_HandleTypeDef *hi2s);

/**
 * @brief Mute/unmute PCM5102A output
 * @param mute 1 = mute, 0 = unmute
 */
void PCM5102A_SetMute(uint8_t mute);

/**
 * @brief Test I2S transmission with triangle wave
 * @param hi2s Pointer to I2S handle
 * @retval HAL status
 */
HAL_StatusTypeDef Test_I2S_TriangleWave(I2S_HandleTypeDef *hi2s);

/**
 * @brief Test I2S transmission with sine wave
 * @param hi2s Pointer to I2S handle
 * @retval HAL status
 */
HAL_StatusTypeDef Test_I2S_SineWave(I2S_HandleTypeDef *hi2s);

/**
 * @brief Start continuous I2S audio playback using DMA
 * @param hi2s Pointer to I2S handle
 * @param buffer Audio buffer (stereo samples)
 * @param size Buffer size in samples (not bytes)
 * @retval HAL status
 */
HAL_StatusTypeDef I2S_StartContinuous(I2S_HandleTypeDef *hi2s, uint16_t *buffer, uint16_t size);

/**
 * @brief Print I2S configuration for debugging
 * @param hi2s Pointer to I2S handle
 */
void I2S_PrintConfig(I2S_HandleTypeDef *hi2s);

/**
 * @brief Check I2S pin connections and status
 * @param hi2s Pointer to I2S handle
 * @retval 1 if OK, negative error code if problem detected
 */
int I2S_DiagnoseConnections(I2S_HandleTypeDef *hi2s);

#ifdef __cplusplus
}
#endif

#endif /* __I2S_AUDIO_H */
