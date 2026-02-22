/**
 ******************************************************************************
 * @file           : swo_log.h
 * @brief          : SWO/ITM logging macros for debugging via SWD
 ******************************************************************************
 * @attention
 *
 * Use these macros to print debug messages through SWD without UART.
 * Messages appear in OpenOCD or IDE console when SWO is enabled.
 *
 * Usage:
 *   SWO_Init();           // Call once in main() after clock init
 *   SWO_PrintString("Hello World\n");
 *   SWO_Printf("Value: %d\n", 42);
 *
 ******************************************************************************
 */

#ifndef __SWO_LOG_H
#define __SWO_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>
#include "stm32h5xx_hal.h"

/* ITM register addresses */
#define ITM_STIM0       (*((volatile uint32_t *)0xE0000000))  /* ITM Stimulus Port 0 */
#define ITM_TER         (*((volatile uint32_t *)0xE0000E00))  /* ITM Trace Enable */
#define ITM_TCR         (*((volatile uint32_t *)0xE0000E80))  /* ITM Trace Control */

#define DWT_CTRL        (*((volatile uint32_t *)0xE0001000))  /* DWT Control */

#define DBGMCU_CR       (*((volatile uint32_t *)0xE0044004))  /* Debug MCU Config */

/**
 * @brief Initialize SWO/ITM for debug output
 * @note Call this after SystemClock_Config() in main()
 */
static inline void SWO_Init(void)
{
    /* Enable TRCENA bit in Debug Exception and Monitor Control Register */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    
    /* Enable ITM */
    ITM_TCR = (1 << 0) |    /* ITM Enable */
              (1 << 3);      /* Enable sync packets */
    
    /* Enable ITM Stimulus Port 0 */
    ITM_TER = 0x00000001;   /* Enable stimulus port 0 */
    
    /* DWT_CTRL for cycle counter (optional, useful for timing) */
    DWT_CTRL |= (1 << 0);   /* Enable cycle counter */
}

/**
 * @brief Print single character to SWO
 * @param c Character to print
 */
static inline void SWO_PrintChar(char c)
{
    /* Wait until ITM port is ready */
    while ((ITM_STIM0 & 0x00000001) == 0);
    
    /* Write character to ITM stimulus port 0 */
    ITM_STIM0 = c;
}

/**
 * @brief Print string to SWO
 * @param str Null-terminated string
 */
static inline void SWO_PrintString(const char *str)
{
    while (*str) {
        SWO_PrintChar(*str++);
    }
}

/**
 * @brief Printf-style formatted output to SWO
 * @param format Format string (like printf)
 * @param ... Variable arguments
 */
static inline void SWO_Printf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    SWO_PrintString(buffer);
}

/**
 * @brief Print hex dump of memory region
 * @param data Pointer to data
 * @param len Length in bytes
 */
static inline void SWO_PrintHex(const uint8_t *data, uint32_t len)
{
    char buffer[64];
    for (uint32_t i = 0; i < len; i++) {
        snprintf(buffer, sizeof(buffer), "%02X ", data[i]);
        SWO_PrintString(buffer);
        if ((i + 1) % 16 == 0) {
            SWO_PrintChar('\n');
        }
    }
    if (len % 16 != 0) {
        SWO_PrintChar('\n');
    }
}

/* Convenience macros */
#define LOG_INFO(msg)           SWO_PrintString("[INFO] " msg "\n")
#define LOG_ERROR(msg)          SWO_PrintString("[ERROR] " msg "\n")
#define LOG_DEBUG(msg)          SWO_PrintString("[DEBUG] " msg "\n")
#define LOG_WARN(msg)           SWO_PrintString("[WARN] " msg "\n")

#define LOG_INFO_FMT(fmt, ...)  SWO_Printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define LOG_ERROR_FMT(fmt, ...) SWO_Printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define LOG_DEBUG_FMT(fmt, ...) SWO_Printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN_FMT(fmt, ...)  SWO_Printf("[WARN] " fmt "\n", ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __SWO_LOG_H */
