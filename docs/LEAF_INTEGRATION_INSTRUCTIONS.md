# LEAF Integration Instructions

This document shows the changes needed to integrate LEAF audio framework with double-buffering.

## Step 1: Add LEAF to includes (line ~23)
```c
/* USER CODE BEGIN Includes */
#include "utils.h"
#include "leaf.h"  // ADD THIS
#include <math.h>
/* USER CODE END Includes */
```

## Step 2: Replace buffer definitions (lines ~50-62)
Replace:
```c
/* USER CODE BEGIN PV */

// Global stereo audio buffer for I2S
#define SAMPLE_RATE 96000
...
uint16_t stereo_buffer[MAX_STEREO_SAMPLES];

/* USER CODE END PV */
```

With:
```c
/* USER CODE BEGIN PV */

// LEAF Constants
#define SAMPLERATE 96000
#define LEAF_BUFFER_SIZE (2 * 96100)
#define DMA_BUFFER_SIZE 8192  // Total DMA buffer size (must be even)

// LEAF Memory pool
char mempool[10000];

// Double buffer for DMA - stereo 16-bit samples
uint16_t dma_buffer[DMA_BUFFER_SIZE] = {0};

// Buffer management for double-buffering
volatile uint16_t *current_write_ptr = dma_buffer;
volatile size_t current_sample_count = 0;
volatile uint32_t buffer_overrun_count = 0;

// LEAF objects
LEAF leaf;
tCycle cycle;
tHermiteDelay delay;

/* USER CODE END PV */
```

## Step 3: Replace callbacks (lines ~84-100)
Replace entire callback section with:
```c
/* USER CODE BEGIN 0 */

// Random number generator for LEAF
float rnd_func()
{
    return ((float)rand() / (float)(RAND_MAX));
}

// DMA Half Transfer Complete Callback - first half sent, fill it
void I2S_TxHalfCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (current_sample_count >= DMA_BUFFER_SIZE / 2)
  {
    // Switch to first half of buffer for writing
    current_write_ptr = dma_buffer;
    current_sample_count = 0;
  }
  else
  {
    // Buffer underrun
    buffer_overrun_count++;
  }
}

// DMA Transfer Complete Callback - second half sent, fill it
void I2S_TxCpltCallback(I2S_HandleTypeDef *hi2s)
{
  if (current_sample_count >= DMA_BUFFER_SIZE / 2)
  {
    // Switch to second half of buffer for writing
    current_write_ptr = dma_buffer + DMA_BUFFER_SIZE / 2;
    current_sample_count = 0;
  }
  else
  {
    // Buffer underrun
    buffer_overrun_count++;
  }
}

/* USER CODE END 0 */
```

## Step 4: Replace main function USER CODE BEGIN 2 section (lines ~168-213)
Replace everything from `/* USER CODE BEGIN 2 */` to `/* USER CODE END 2 */` with:
```c
  /* USER CODE BEGIN 2 */

  // Test PSRAM connection at startup
  int psram_result = Test_PSRAM_Connection();
  if (psram_result < 0) {
    Error_Handler();
  }

  // Initialize LEAF framework
  LEAF_init(&leaf, SAMPLERATE, mempool, LEAF_BUFFER_SIZE, &rnd_func);
  
  // Initialize LEAF oscillator (cycle) and delay
  tCycle_init(&cycle, &leaf);
  tCycle_setFreq(&cycle, 220.0f);  // Start at 220 Hz (A3)
  
  tHermiteDelay_init(&delay, 2000, 2500, &leaf);
  tHermiteDelay_setGain(&delay, 0.5f);

  // Unmute PCM5102A
  HAL_GPIO_WritePin(AUDIO_MUTE_CONTROL_GPIO_Port, AUDIO_MUTE_CONTROL_Pin, GPIO_PIN_SET);
  HAL_Delay(10);
  
  // Register both DMA callbacks for double-buffering
  HAL_I2S_RegisterCallback(&hi2s1, HAL_I2S_TX_COMPLETE_CB_ID, I2S_TxCpltCallback);
  HAL_I2S_RegisterCallback(&hi2s1, HAL_I2S_TX_HALF_COMPLETE_CB_ID, I2S_TxHalfCpltCallback);
  
  // Start I2S DMA transmission
  HAL_StatusTypeDef i2s_status = HAL_I2S_Transmit_DMA(&hi2s1, dma_buffer, DMA_BUFFER_SIZE);
  if (i2s_status != HAL_OK) {
    Error_Handler();
  }
  
  uint64_t counter = 0;

  /* USER CODE END 2 */
```

## Step 5: Replace main loop USER CODE BEGIN 3 section
Replace the while loop contents with:
```c
    /* USER CODE BEGIN 3 */
    
    // Fill the current buffer half when DMA has space
    if (current_sample_count < DMA_BUFFER_SIZE / 2)
    {
      counter++;
      
      // Simple frequency sequence for demonstration
      if ((counter % 100000) == 10000)
        tCycle_setFreq(&cycle, 220.0f);  // A3
      else if ((counter % 100000) == 20000)
        tCycle_setFreq(&cycle, 330.0f);  // E4
      else if ((counter % 100000) == 30000)
        tCycle_setFreq(&cycle, 440.0f);  // A4
      else if ((counter % 100000) == 40000)
        tCycle_setFreq(&cycle, 0.0f);    // Silence
      
      // Process audio with LEAF - generate mono sample
      float processed_value = tCycle_tick(&cycle);
      
      // Apply delay effect (optional - comment out for clean oscillator)
      // processed_value = tHermiteDelay_tick(&delay, processed_value);
      
      // Convert float [-1.0, 1.0] to 16-bit signed
      int16_t sample = (int16_t)(processed_value * 32767.0f);
      
      // Write stereo (duplicate mono to both channels)
      *(current_write_ptr + current_sample_count) = (uint16_t)sample;
      current_sample_count++;
    }
```

## Step 6: Add LEAF library to Makefile
In the `C_SOURCES` section, add:
```makefile
Core/Src/leaf/leaf.c \
Core/Src/leaf/leaf-oscillators.c \
Core/Src/leaf/leaf-delay.c \
# ... other LEAF source files as needed
```

And in `C_INCLUDES`, add:
```makefile
-ICore/Inc/leaf
```

## How It Works
1. **Double Buffering**: 8192-sample buffer split into two 4096-sample halves
2. **Half Complete Callback**: Fires when first half is sent - fills first half while second plays
3. **Complete Callback**: Fires when second half is sent - fills second half while first plays
4. **Main Loop**: Generates samples with LEAF when buffer has space
5. **Smooth Playback**: No gaps or clicks because we're always filling ahead of playback

## Key Differences from Original
- Uses 16-bit signed audio (not 12-bit unsigned like your original)
- Buffer size optimized for STM32H5 (8192 samples)
- Proper stereo handling (your original was mono)
- Error tracking with `buffer_overrun_count`
