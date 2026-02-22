# Debugging Troubleshooting for STM32H533

## Current Status

Your OpenOCD setup is **mostly working** but encounters a **CPU examination failure** due to STM32H5 secure watermark protection that persists in option bytes.

### What's Working ✅
- ST-Link connection established
- SWD communication functional  
- Flash programming via STM32CubeProgrammer successful
- AP2 (Access Port 2) examination succeeds
- GDB server starts on port 3333

### What's Not Working ❌
- **CPU examination fails** - cannot read memory at 0xe000ed00 (System Control Block)
- **Secure watermarks persist** - SECWM1/SECWM2 protection enabled despite firmware not configuring it
- **Debug session cannot start** in VS Code due to target not examined

## Root Cause

The STM32H533 has **Secure Watermark (SECWM)** option bytes that define protected flash regions. Your chip has:

```
SECWM1_STRT: 0x0  (0x8000000) 
SECWM1_END:  0x1C (0x8038000)  ← Protecting 224KB of Bank 1

SECWM2_STRT: 0x0  (0x8040000)
SECWM2_END:  0x1C (0x8078000)  ← Protecting 224KB of Bank 2
```

When `STRT ≤ END`, the watermark is **active** and prevents debug access to the CPU core, even though:
- TrustZone is disabled (TZEN = 0xC3)
- Product state is "Open" (0xED)
- Your firmware doesn't configure watermarks

## Why Watermarks Won't Clear

You've tried multiple times to disable watermarks by setting `SECWM_STRT > SECWM_END`, but the values **revert to 0x0/0x1C**. This happens because:

1. **Factory defaults** - STM32H5 chips may ship with watermarks pre-configured
2. **Option byte persistence** - These are burned into flash option bytes, not RAM
3. **STM32CubeProgrammer limitations** - Cannot set all values (rejects 0x7F as invalid)
4. **Hardware security** - Once set, some security features resist software clearing

## Workarounds

### Option 1: Use STM32CubeProgrammer for Debugging (Recommended)

STM32CubeProgrammer can connect successfully despite watermarks. Use it instead of OpenOCD:

1. In VS Code, use the **"Debug (STLink GDB Server)"** configuration (if Cortex-Debug supports it)
2. Or debug directly in STM32CubeIDE which handles H5 security properly
3. STM32CubeProgrammer CLI works for flash operations: `make flash`

### Option 2: Regression to Factory State (Risky)

Some STM32H5 chips support "regression" to clear all security:

```bash
~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI \
  -c port=SWD mode=UR -ob rdp=0xAA
```

**⚠️ WARNING**: This may irreversibly lock the chip on some H5 variants. Test on non-critical hardware first.

### Option 3: OpenOCD with Limited Functionality

OpenOCD can still be useful for:
- **Flash programming** (works via AP2)
- **Memory dumps** (CubeProgrammer works better)
- **Some GDB operations** (with limitations)

But it **cannot**:
- Halt the CPU reliably
- Single-step debugging
- Breakpoint management
- Read CPU registers

### Option 4: Hardware Debugging via SWO

Since SWD data channel works, you can use **SWO (Serial Wire Output)** for printf-style debugging:

```c
#include "swo_log.h"

int main(void) {
    HAL_Init();
    SystemClock_Config();
    SWO_Init();  // Enable SWO logging
    
    LOG_INFO("System started");
    SWO_Printf("Counter: %d\n", value);
}
```

This works **without** needing CPU halt! SWO uses a separate trace pin.

## Recommended Path Forward

**For immediate debugging needs:**

1. **Use SWO logging** (`swo_log.h` already created) for runtime debugging
2. **Use STM32CubeProgrammer** for flashing: `make flash`
3. **Use STM32CubeIDE** for full debugging (it handles H5 security)

**For proper OpenOCD debugging:**

1. Contact ST Support about clearing persistent watermarks on your specific chip revision
2. Check if your chip supports option byte regression
3. Consider using a fresh STM32H533 chip that hasn't had security configured

**For production:**

Keep watermarks **disabled** in your firmware (already done - FLASH peripheral disabled in CubeMX).

## Files Created

- `openocd.cfg` - Simplified OpenOCD config for STM32H533 (with H7 fallback)
- `Core/Inc/swo_log.h` - SWO/ITM logging macros for printf via SWD
- `DEBUG_SETUP.md` - Full debugging guide with SWO instructions
- `Makefile` targets:
  - `make flash` - Flash firmware with CubeProgrammer
  - `make clear-protection` - Attempt to erase flash (watermarks persist)

## Next Steps

**Option A - Use SWO Logging (Simplest)**
1. Add `#include "swo_log.h"` to your main.c
2. Call `SWO_Init()` after `SystemClock_Config()`
3. Use `LOG_INFO()`, `SWO_Printf()` etc. in your code
4. Rebuild: `make -j && make flash`
5. View output in OpenOCD telnet or dedicated SWO viewer

**Option B - Debug in STM32CubeIDE**
1. Open project in STM32CubeIDE (it handles H5 security)
2. Use built-in debugger (works with watermarks)
3. Full breakpoint/step debugging available

**Option C - Continue Investigating OpenOCD**
1. Check with ST Support about clearing watermarks
2. Try option byte regression (risky)
3. Test on a different STM32H533 chip

## References

- Your successful CubeProgrammer command: `~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD mode=UR -r 0x08000000 0x80000 flash_dump.bin`
- OpenOCD issue discussion: Your secure watermarks prevent CPU examination but AP2 works
- STM32H5 Security Guide: [AN5924](https://www.st.com/resource/en/application_note/an5924-getting-started-with-stm32h5-trustzone-stmicroelectronics.pdf)
