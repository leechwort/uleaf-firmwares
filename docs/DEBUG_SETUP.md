# STM32H533 Debug Setup Guide

## Overview
This project is configured for debugging with OpenOCD and VS Code. The STM32H5 series has security features (secure watermarks) that can block debugging. This guide explains how to resolve those issues.

## Problem: Secure Watermarks Blocking Debug

The STM32H533 has SECWM1/SECWM2 (Secure Watermark) registers that can prevent debug access. Symptoms:
- OpenOCD reports "target examination failed"
- Cannot halt CPU
- GDB connection refused
- Error reading memory at 0xe000ed00

## Solution Steps

### 1. Clear Secure Watermarks (One-Time Fix)

Run this command to clear the protection:
```bash
make clear-protection
```

This sets SECWM_STRT > SECWM_END which disables the watermark protection.

### 2. Prevent Firmware from Re-Enabling Protection

Check your code (likely in `main.c` or CubeMX-generated files) for:
```c
// REMOVE or COMMENT OUT:
HAL_FLASHEx_ConfigSecureWatermark(...)
// Or any SECWM register writes
```

In STM32CubeMX:
- System Core → FLASH should be **DISABLED** (or security features disabled)
- This prevents watermark configuration code from being generated

### 3. Build and Flash

```bash
# Build firmware
make -j

# Flash with automatic watermark clearing
make flash
```

The `flash` target automatically:
1. Builds the firmware
2. Connects under reset (prevents existing firmware from blocking)
3. Flashes new firmware
4. Resets the device

## Debugging with VS Code

### Start Debugging

1. Press **F5** or click "Run → Start Debugging"
2. Select "Debug (Cortex-Debug)" configuration
3. OpenOCD will start automatically and connect to your board

### Available Debug Configurations

- **Debug (Cortex-Debug)**: Full debugging with OpenOCD (recommended)
- **Debug (OpenOCD)**: Alternative using cppdbg

### VS Code Tasks

- **Ctrl+Shift+B**: Build firmware
- **Tasks: Run Task**:
  - `Build Debug` - Compile firmware
  - `Clean Build` - Remove build artifacts
  - `Start OpenOCD` - Manually start OpenOCD server
  - `Stop OpenOCD` - Stop OpenOCD server
  - `Flash Device` - Flash firmware to MCU
  - `Clear Protection` - Clear secure watermarks

## SWO Logging (Print via SWD)

You can print debug messages through SWD without UART using the ITM (Instrumentation Trace Macrocell).

### Setup in Code

```c
#include "swo_log.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    
    // Initialize SWO logging
    SWO_Init();
    
    // Use logging macros
    LOG_INFO("System initialized");
    LOG_DEBUG_FMT("CPU frequency: %lu Hz", SystemCoreClock);
    SWO_Printf("Counter: %d\n", 42);
    
    while (1) {
        // Your code
    }
}
```

### View SWO Output in VS Code

SWO output is configured in the launch.json. When debugging:
1. Start debug session (F5)
2. Open "OUTPUT" panel (View → Output)
3. Select "Adapter Output (Cortex-Debug)" from dropdown
4. You'll see ITM messages in real-time

### SWO Logging API

```c
// Basic functions
SWO_Init();                          // Initialize (call once)
SWO_PrintChar('A');                  // Print single character
SWO_PrintString("Hello\n");          // Print string
SWO_Printf("Value: %d\n", 123);      // Printf-style

// Convenience macros
LOG_INFO("System started");                    // [INFO] prefix
LOG_ERROR_FMT("Error code: %d", err);         // [ERROR] with formatting
LOG_DEBUG("Debug message");                    // [DEBUG] prefix
LOG_WARN("Warning message");                   // [WARN] prefix

// Hex dump
uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
SWO_PrintHex(data, sizeof(data));
```

### SWO Configuration

The SWO is configured for:
- **CPU Frequency**: 250 MHz (adjust in launch.json if different)
- **SWO Frequency**: 2 MHz
- **ITM Port 0**: Console output

If you change the CPU clock frequency, update `launch.json`:
```json
"swoConfig": {
    "cpuFrequency": 250000000,  // Update this
    "swoFrequency": 2000000
}
```

## Troubleshooting

### OpenOCD can't connect
```bash
# Check if ST-Link is detected
lsusb | grep STMicro

# Verify user permissions
sudo usermod -a -G dialout $USER
# Log out and back in for group change to take effect

# Try connecting under reset
openocd -f openocd.cfg -c "init" -c "reset halt"
```

### CPU examination fails
```bash
# Clear secure watermarks
make clear-protection

# Then rebuild and flash
make flash
```

### GDB connection refused
```bash
# Manually start OpenOCD first
make Start\ OpenOCD

# Check if port 3333 is listening
ss -tlnp | grep 3333

# Then try debugging again
```

### SWO output not appearing

1. Verify SWO_Init() is called after SystemClock_Config()
2. Check CPU frequency in launch.json matches actual frequency
3. Ensure trace pins are not used for other functions
4. Try reducing SWO frequency to 1 MHz

### Build fails with "undefined reference"

```bash
# Clean and rebuild
make clean
make -j
```

### Flash memory appears erased after debugging

This is normal - secure watermarks were preventing proper flash operations. After clearing them, the flash should program correctly.

## OpenOCD Configuration Details

The `openocd.cfg` file includes:
- **Connect under reset**: Prevents firmware from blocking debug
- **gdb_memory_map disable**: Workaround for H5 examination failure
- **adapter speed 4000**: 4 MHz SWD clock
- **srst_only**: Uses hardware reset (NRST pin)

## Hardware Requirements

- ST-Link V2 or compatible debugger
- Proper connections:
  - SWDIO (PA13)
  - SWCLK (PA14)
  - GND
  - NRST (recommended for "connect under reset")
  - VDD (for target voltage detection)

## Additional Resources

- [OpenOCD Documentation](http://openocd.org/doc/)
- [Cortex-Debug Extension](https://github.com/Marus/cortex-debug)
- [STM32H5 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0481-stm32h533-stm32h523-and-stm32h562-armbased-32bit-mcus-stmicroelectronics.pdf)
- [STM32H5 Security Features](https://wiki.st.com/stm32mcu/wiki/Security:Introduction_to_security)

## Quick Reference

```bash
# Build
make -j

# Flash
make flash

# Clean
make clean

# Clear protection (if debugging fails)
make clear-protection

# Debug in VS Code
# Press F5
```

## Notes

- Keep FLASH peripheral **DISABLED** in CubeMX to prevent watermark configuration
- Always use "connect under reset" mode when flashing
- SWO logging has ~5% performance impact (negligible for most applications)
- Secure watermarks persist across resets but can be cleared via SWD
