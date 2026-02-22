# PCM5102A I2S Audio Troubleshooting Guide

## Your Current Setup

**PCM5102A Connections:**
- BCK  (Bit Clock)    ← I2S1_CK  (PA5)
- DIN  (Data In)      ← I2S1_SD  (PA7)
- LRCK (LR Clock/WS)  ← **I2S2_WS (PB12)** ⚠️ **THIS IS WRONG!**
- XSMT (Mute Control) ← PC15

**PCM5102A Pins to GND:**
- SCK, FLT, DEMP, FMT = GND

## 🔴 Critical Issue Found!

You said **"I2S2_WS connected to LRCK"** but you're using **I2S1**!

### The Problem:
- **I2S1_WS** should be on **PA4** or **PA15** (depending on pin mapping)
- **I2S2_WS** is on **PB12** (different peripheral!)
- If you connected PB12 to LRCK, but you're using I2S1, then LRCK is getting NO signal!

### Solution:
You need to connect **I2S1_WS** (not I2S2_WS) to PCM5102A LRCK pin.

**Check your STM32CubeMX configuration:**
1. Open your `.ioc` file in CubeMX
2. Go to Connectivity → SPI1 (I2S1)
3. Check which pin is assigned to I2S1_WS
4. That pin must be connected to PCM5102A LRCK!

## Common I2S1 Pin Mappings for STM32H533

| Signal   | Possible Pins       | Your Config? |
|----------|---------------------|--------------|
| I2S1_CK  | PA5, PB3            | PA5 ✓        |
| I2S1_SD  | PA7, PB5            | PA7 ✓        |
| I2S1_WS  | PA4, PA15           | ??? ⚠️       |

## Other Issues I Fixed in Your Code

### 1. FirstBit was set to LSB (wrong!)
**Problem:** I2S standard uses MSB first
**Fixed:** Changed to `I2S_FIRSTBIT_MSB`

### 2. Audio frequency was too low
**Problem:** 8 kHz sounds poor quality
**Fixed:** Changed to `I2S_AUDIOFREQ_48K` (48 kHz)

### 3. Not creating stereo data
**Problem:** PCM5102A expects interleaved stereo: [L, R, L, R, ...]
**Fixed:** Code now creates proper stereo buffer

### 4. Mute pin might be in wrong state
**Problem:** XSMT LOW = muted (no audio)
**Fixed:** Code sets it HIGH to unmute

## How to Diagnose

Flash the updated firmware and check SWO output (requires SWO logging setup):

```c
SWO_Init();
I2S_DiagnoseConnections(&hi2s1);
```

This will print:
- I2S configuration
- Mute pin state
- Pin connection warnings

## Testing Steps

### Step 1: Verify Pin Connections
```
I2S1_CK  (PA5)  → PCM5102A BCK  ✓
I2S1_SD  (PA7)  → PCM5102A DIN  ✓
I2S1_WS  (PA4?) → PCM5102A LRCK ⚠️ CHECK THIS!
PC15            → PCM5102A XSMT ✓
```

### Step 2: Check with Oscilloscope

**On BCK (PA5):**
- Should see continuous clock at ~1.5 MHz (for 48 kHz * 32 bits)
- Clock should run all the time when transmitting

**On WS/LRCK:**
- Should see square wave at 48 kHz (audio sample rate)
- Toggles between left and right channel
- **If you see NOTHING here → wrong pin connected!**

**On DIN (PA7):**
- Should see data synchronized with BCK
- Data changes on BCK edges

**On XSMT (PC15):**
- Should be HIGH (3.3V) for audio to play
- If LOW → PCM5102A is muted!

### Step 3: Check PCM5102A Power

- VCC = 3.3V or 5V (check datasheet for your module)
- All GND pins connected
- AVCC = VCC
- CPVCC = VCC

### Step 4: Check Audio Output

PCM5102A outputs:
- LOUT (Left channel)
- ROUT (Right channel)  
- GND (ground reference)

Connect to:
- Headphones (with 1µF AC coupling caps if needed)
- Powered speakers
- Oscilloscope to see waveform

## Quick Test Commands

```bash
# Build
make -j

# Flash
make flash

# View debug output (if SWO configured)
# You should see I2S config and diagnostics
```

## Expected Behavior

With the fixed code, you should hear a **triangle wave tone** continuously from both left and right channels.

## If Still No Audio

### Check 1: Measure voltages
```
PC15 (XSMT): Should be 3.3V (HIGH = unmuted)
PA5 (BCK):   Should see clock signal (~1.5 MHz)
PA4 (WS):    Should see 48 kHz square wave ⚠️ CHECK YOUR WS PIN!
PA7 (DIN):   Should see data pulses
```

### Check 2: Verify I2S is actually transmitting
Add this to check HAL status:
```c
HAL_StatusTypeDef status = HAL_I2S_Transmit(&hi2s1, stereo_buffer, 64, 1000);
if (status != HAL_OK) {
    printf("I2S Error: %d\n", status);
}
```

### Check 3: PCM5102A module vs chip
Some PCM5102A **modules** have onboard pullups/pulldowns. Check if your module:
- Has FLT pulled up/down
- Has DEMP configured  
- Has FMT configured
- Has SCK input or pulled down

### Check 4: Clock frequency
At 48 kHz, 16-bit extended (32-bit frame), stereo:
```
Bit clock = 48000 Hz × 32 bits × 2 channels = 3.072 MHz
```

Your PLL must generate correct clock for I2S peripheral.

## Fix in STM32CubeMX

1. Open `basic-project.ioc`
2. Go to **Connectivity → SPI1**
3. Set **Mode** to **Full-Duplex Master** (then switch to I2S)
4. Or use **I2S Mode: Master Transmit**
5. **CRITICAL:** Check the **I2S1_WS** pin assignment!
6. Make sure it's **PA4** or **PA15** (not PB12!)
7. Connect **that pin** to PCM5102A LRCK
8. Regenerate code
9. Rebuild and flash

## Summary of Changes Made

✅ Fixed `I2S_FIRSTBIT_MSB` (was LSB)  
✅ Fixed audio frequency 48 kHz (was 8 kHz)  
✅ Added proper stereo buffer creation  
✅ Added PCM5102A init with unmute  
✅ Added I2S diagnostic functions  
✅ Added SWO logging support  

⚠️ **YOU NEED TO FIX:** I2S1_WS pin connection!

## Files Created/Modified

- `Core/Inc/i2s_audio.h` - I2S utilities and PCM5102A functions
- `Core/Src/i2s_audio.c` - Implementation with test waveforms
- `Core/Src/main.c` - Fixed I2S config, proper stereo data, unmute logic
- `Makefile` - Added i2s_audio.c to build

## Next Steps

1. **Fix the WS pin connection!** This is your main issue.
2. Flash the updated firmware: `make flash`
3. Check oscilloscope on all I2S pins
4. If LRCK (WS) has no signal → wrong pin connected!
5. Check XSMT is HIGH (unmuted)
6. Should hear triangle wave tone

## Additional Resources

- PCM5102A Datasheet: https://www.ti.com/lit/ds/symlink/pcm5102a.pdf
- STM32H5 I2S Application Note: https://www.st.com/resource/en/application_note/an4872-how-to-use-the-i2s-audio-protocol-with-stm32-mcus-stmicroelectronics.pdf
