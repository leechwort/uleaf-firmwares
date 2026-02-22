#!/bin/bash
# Pre-flash script: Erase flash sectors to avoid watermark verification issues

PROGRAMMER=~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI

echo "=========================================="
echo "Pre-Flash: Erasing flash sectors..."
echo "=========================================="

# Erase only the sectors we're about to program (0-4 for 38KB firmware)
$PROGRAMMER -c port=SWD mode=UR -e 0 1 2 3 4 > /dev/null 2>&1

if [ $? -eq 0 ]; then
    echo "✓ Flash sectors erased successfully"
    exit 0
else
    echo "✗ Flash erase failed (may still work)"
    exit 0  # Don't block the flash - let it try anyway
fi
