#!/bin/bash

# STM32H5 Security Watermark Fix Script
# This script disables security watermarks that prevent debugging

echo "=========================================="
echo "STM32H5 Security Watermark Remover"
echo "=========================================="
echo ""

# Set STM32CubeProgrammer path
STM32_PROG="$HOME/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI"

# Check if STM32_Programmer_CLI is available
if [ ! -f "$STM32_PROG" ]; then
    echo "ERROR: STM32_Programmer_CLI not found at: $STM32_PROG"
    echo "Please install STM32CubeProgrammer"
    echo "Download from: https://www.st.com/en/development-tools/stm32cubeprog.html"
    exit 1
fi

echo "Step 1: Connecting to STM32H533..."
"$STM32_PROG" -c port=SWD mode=UR reset=HWrst
if [ $? -ne 0 ]; then
    echo "ERROR: Failed to connect. Make sure:"
    echo "  - ST-Link is connected"
    echo "  - Board is powered"
    echo "  - No other debugger is running"
    exit 1
fi

echo ""
echo "Step 2: Reading current option bytes..."
"$STM32_PROG" -c port=SWD mode=UR -ob displ

echo ""
echo "Step 3: Performing mass erase to clear security watermarks..."
echo "WARNING: This will erase all flash memory!"
read -p "Continue? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Aborted by user"
    exit 1
fi

"$STM32_PROG" -c port=SWD mode=UR reset=HWrst -e all
if [ $? -ne 0 ]; then
    echo "ERROR: Mass erase failed"
    exit 1
fi

echo ""
echo "Step 4: Disabling RDP (Readout Protection)..."
"$STM32_PROG" -c port=SWD mode=UR -ob RDP=0xAA
if [ $? -ne 0 ]; then
    echo "WARNING: RDP disable may have failed, continuing..."
fi

echo ""
echo "Step 5: Clearing security watermarks (SECWM)..."
# Set SECWM1_STRT and SECWM1_END to 0x7F (no secure area)
# Set SECWM2_STRT and SECWM2_END to 0x7F (no secure area)
"$STM32_PROG" -c port=SWD mode=UR -ob SECWM1_STRT=0x7F SECWM1_END=0x00
if [ $? -ne 0 ]; then
    echo "WARNING: SECWM1 clear may have failed"
fi

"$STM32_PROG" -c port=SWD mode=UR -ob SECWM2_STRT=0x7F SECWM2_END=0x00
if [ $? -ne 0 ]; then
    echo "WARNING: SECWM2 clear may have failed"
fi

echo ""
echo "Step 6: Verifying option bytes..."
"$STM32_PROG" -c port=SWD mode=UR -ob displ

echo ""
echo "Step 7: Disconnecting..."
"$STM32_PROG" -c port=SWD mode=UR -hardRst

echo ""
echo "=========================================="
echo "Watermark removal complete!"
echo "=========================================="
echo ""
echo "You can now flash your firmware:"
echo "  make flash"
echo ""
echo "Or test OpenOCD connection:"
echo "  openocd -f openocd.cfg"
echo ""
