/**
 ******************************************************************************
 * @file           : i2s_audio.c
 * @brief          : I2S audio utility functions for PCM5102A debugging
 ******************************************************************************
 */

#include "i2s_audio.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

/* 440Hz sine wave at 48kHz sample rate (one complete period) */
const uint16_t sine_wave_440hz_48k[] = {
    0x8000, 0x8c8b, 0x98f8, 0xa527, 0xb0fb, 0xbc56, 0xc71c, 0xd133,
    0xda82, 0xe2f1, 0xea6d, 0xf0e2, 0xf641, 0xfa7c, 0xfd89, 0xff62,
    0xffff, 0xff62, 0xfd89, 0xfa7c, 0xf641, 0xf0e2, 0xea6d, 0xe2f1,
    0xda82, 0xd133, 0xc71c, 0xbc56, 0xb0fb, 0xa527, 0x98f8, 0x8c8b,
    0x8000, 0x7375, 0x6708, 0x5ad9, 0x4f05, 0x43aa, 0x38e4, 0x2ecd,
    0x257e, 0x1d0f, 0x1593, 0x0f1e, 0x09bf, 0x0584, 0x0277, 0x009e,
    0x0001, 0x009e, 0x0277, 0x0584, 0x09bf, 0x0f1e, 0x1593, 0x1d0f,
    0x257e, 0x2ecd, 0x38e4, 0x43aa, 0x4f05, 0x5ad9, 0x6708, 0x7375
};

const uint32_t sine_wave_440hz_48k_size = sizeof(sine_wave_440hz_48k) / sizeof(sine_wave_440hz_48k[0]);

/* Simple triangle wave for testing */
const uint16_t triangle_wave[] = {
    0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000,
    0x8000, 0x9000, 0xa000, 0xb000, 0xc000, 0xd000, 0xe000, 0xf000,
    0xffff, 0xf000, 0xe000, 0xd000, 0xc000, 0xb000, 0xa000, 0x9000,
    0x8000, 0x7000, 0x6000, 0x5000, 0x4000, 0x3000, 0x2000, 0x1000
};

const uint32_t triangle_wave_size = sizeof(triangle_wave) / sizeof(triangle_wave[0]);

/**
 * @brief Initialize PCM5102A DAC
 */
HAL_StatusTypeDef PCM5102A_Init(I2S_HandleTypeDef *hi2s)
{
    // Unmute the PCM5102A (XSMT high = unmute)
    PCM5102A_SetMute(0);
    
    // Small delay to let DAC stabilize
    HAL_Delay(10);
    
    return HAL_OK;
}

/**
 * @brief Mute/unmute PCM5102A
 */
void PCM5102A_SetMute(uint8_t mute)
{
    if (mute) {
        // XSMT LOW = mute
        HAL_GPIO_WritePin(AUDIO_MUTE_CONTROL_GPIO_Port, AUDIO_MUTE_CONTROL_Pin, GPIO_PIN_RESET);
    } else {
        // XSMT HIGH = unmute
        HAL_GPIO_WritePin(AUDIO_MUTE_CONTROL_GPIO_Port, AUDIO_MUTE_CONTROL_Pin, GPIO_PIN_SET);
    }
}

/**
 * @brief Test I2S with triangle wave
 */
HAL_StatusTypeDef Test_I2S_TriangleWave(I2S_HandleTypeDef *hi2s)
{
    HAL_StatusTypeDef status;
    
    // Transmit triangle wave
    status = HAL_I2S_Transmit(hi2s, (uint16_t*)triangle_wave, triangle_wave_size, 1000);
    
    return status;
}

/**
 * @brief Test I2S with sine wave
 */
HAL_StatusTypeDef Test_I2S_SineWave(I2S_HandleTypeDef *hi2s)
{
    HAL_StatusTypeDef status;
    
    // Transmit sine wave
    status = HAL_I2S_Transmit(hi2s, (uint16_t*)sine_wave_440hz_48k, sine_wave_440hz_48k_size, 1000);
    
    return status;
}

/**
 * @brief Start continuous playback with DMA
 */
HAL_StatusTypeDef I2S_StartContinuous(I2S_HandleTypeDef *hi2s, uint16_t *buffer, uint16_t size)
{
    return HAL_I2S_Transmit_DMA(hi2s, buffer, size);
}

/**
 * @brief Print I2S configuration (requires SWO or UART)
 */
void I2S_PrintConfig(I2S_HandleTypeDef *hi2s)
{
    printf("\n=== I2S Configuration ===\n");
    printf("Mode: %s\n", (hi2s->Init.Mode == I2S_MODE_MASTER_TX) ? "Master TX" : "Other");
    printf("Standard: ");
    switch(hi2s->Init.Standard) {
        case I2S_STANDARD_PHILIPS:      printf("Philips\n"); break;
        case I2S_STANDARD_MSB:          printf("MSB/Left Justified\n"); break;
        case I2S_STANDARD_LSB:          printf("LSB/Right Justified\n"); break;
        case I2S_STANDARD_PCM_SHORT:    printf("PCM Short\n"); break;
        case I2S_STANDARD_PCM_LONG:     printf("PCM Long\n"); break;
        default:                        printf("Unknown\n"); break;
    }
    printf("Data Format: ");
    switch(hi2s->Init.DataFormat) {
        case I2S_DATAFORMAT_16B:        printf("16-bit\n"); break;
        case I2S_DATAFORMAT_16B_EXTENDED: printf("16-bit extended (32-bit frame)\n"); break;
        case I2S_DATAFORMAT_24B:        printf("24-bit\n"); break;
        case I2S_DATAFORMAT_32B:        printf("32-bit\n"); break;
        default:                        printf("Unknown\n"); break;
    }
    printf("Audio Freq: ");
    switch(hi2s->Init.AudioFreq) {
        case I2S_AUDIOFREQ_8K:   printf("8 kHz\n"); break;
        case I2S_AUDIOFREQ_11K:  printf("11.025 kHz\n"); break;
        case I2S_AUDIOFREQ_16K:  printf("16 kHz\n"); break;
        case I2S_AUDIOFREQ_22K:  printf("22.05 kHz\n"); break;
        case I2S_AUDIOFREQ_32K:  printf("32 kHz\n"); break;
        case I2S_AUDIOFREQ_44K:  printf("44.1 kHz\n"); break;
        case I2S_AUDIOFREQ_48K:  printf("48 kHz\n"); break;
        case I2S_AUDIOFREQ_96K:  printf("96 kHz\n"); break;
        case I2S_AUDIOFREQ_192K: printf("192 kHz\n"); break;
        default:                 printf("%lu Hz\n", hi2s->Init.AudioFreq); break;
    }
    printf("MCLK Output: %s\n", (hi2s->Init.MCLKOutput == I2S_MCLKOUTPUT_ENABLE) ? "Enabled" : "Disabled");
    printf("CPOL: %s\n", (hi2s->Init.CPOL == I2S_CPOL_LOW) ? "Low" : "High");
    printf("First Bit: %s\n", (hi2s->Init.FirstBit == I2S_FIRSTBIT_MSB) ? "MSB" : "LSB");
    printf("========================\n\n");
}

/**
 * @brief Diagnose I2S connections
 */
int I2S_DiagnoseConnections(I2S_HandleTypeDef *hi2s)
{
    printf("\n=== I2S Pin Diagnosis ===\n");
    
    // Check if I2S is initialized
    if (hi2s->Instance == NULL) {
        printf("ERROR: I2S not initialized!\n");
        return -1;
    }
    
    printf("I2S Instance: SPI%d\n", (hi2s->Instance == SPI1) ? 1 : 
                                     (hi2s->Instance == SPI2) ? 2 : 
                                     (hi2s->Instance == SPI3) ? 3 : 0);
    
    // Check mute pin state
    GPIO_PinState mute_state = HAL_GPIO_ReadPin(AUDIO_MUTE_CONTROL_GPIO_Port, AUDIO_MUTE_CONTROL_Pin);
    printf("XSMT (Mute Control): %s\n", (mute_state == GPIO_PIN_SET) ? "HIGH (Unmuted)" : "LOW (MUTED!)");
    
    if (mute_state == GPIO_PIN_RESET) {
        printf("  ⚠️  WARNING: PCM5102A is MUTED! Set XSMT high to hear audio.\n");
    }
    
    // Check I2S state
    printf("I2S State: ");
    switch(hi2s->State) {
        case HAL_I2S_STATE_RESET:      printf("RESET\n"); break;
        case HAL_I2S_STATE_READY:      printf("READY\n"); break;
        case HAL_I2S_STATE_BUSY:       printf("BUSY\n"); break;
        case HAL_I2S_STATE_BUSY_TX:    printf("BUSY TX\n"); break;
        case HAL_I2S_STATE_BUSY_RX:    printf("BUSY RX\n"); break;
        case HAL_I2S_STATE_BUSY_TX_RX: printf("BUSY TX/RX\n"); break;
        case HAL_I2S_STATE_TIMEOUT:    printf("TIMEOUT\n"); break;
        case HAL_I2S_STATE_ERROR:      printf("ERROR\n"); break;
        default:                       printf("UNKNOWN\n"); break;
    }
    
    // Common issues checklist
    printf("\n=== Troubleshooting Checklist ===\n");
    printf("1. XSMT pin (mute): %s\n", (mute_state == GPIO_PIN_SET) ? "✓ OK" : "✗ MUTED!");
    printf("2. Data format: %s\n", (hi2s->Init.DataFormat == I2S_DATAFORMAT_16B_EXTENDED) ? "✓ 16-bit extended" : "⚠️  Check format");
    printf("3. First bit: %s\n", (hi2s->Init.FirstBit == I2S_FIRSTBIT_MSB) ? "✓ MSB (correct)" : "⚠️  LSB (may cause issues)");
    printf("4. Audio frequency: %s\n", (hi2s->Init.AudioFreq >= I2S_AUDIOFREQ_8K) ? "✓ Valid" : "✗ Too low");
    printf("5. Standard: %s\n", (hi2s->Init.Standard == I2S_STANDARD_PHILIPS) ? "✓ Philips I2S" : "⚠️  Check standard");
    
    printf("\n=== Pin Connections (verify with schematic) ===\n");
    printf("I2S1_CK  (PA5)  -> PCM5102A BCK  (Bit Clock)\n");
    printf("I2S1_SD  (PA7)  -> PCM5102A DIN  (Data In)\n");
    printf("I2S1_WS  (PA4)  -> PCM5102A LRCK (LR Clock) ⚠️  CHECK THIS!\n");
    printf("PC15            -> PCM5102A XSMT (Mute Control)\n");
    printf("\n⚠️  NOTE: You mentioned I2S2_WS - this is wrong! Should be I2S1_WS (PA4)\n");
    printf("          I2S2 is a different peripheral (PB12). Check your pin configuration!\n");
    
    printf("========================\n\n");
    
    return 1;
}
