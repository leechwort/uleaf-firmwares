# STM32H533 Project - VS Code Debugging Setup

## Prerequisites

Install the following tools:

1. **ARM GCC Toolchain**
   ```bash
   sudo apt install gcc-arm-none-eabi gdb-multiarch
   ```

2. **OpenOCD**
   ```bash
   sudo apt install openocd
   ```

3. **CMake and Ninja**
   ```bash
   sudo apt install cmake ninja-build
   ```

4. **VS Code Extensions**
   - C/C++ Extension Pack (ms-vscode.cpptools-extension-pack)
   - CMake Tools (ms-vscode.cmake-tools)
   - Cortex-Debug (marus25.cortex-debug) - Optional but recommended

## Building the Project

### Option 1: Using VS Code Tasks
- Press `Ctrl+Shift+B` to build (runs "Build Debug" task)
- Or press `Ctrl+Shift+P` and select "Tasks: Run Task" → "Build Debug"

### Option 2: Command Line
```bash
# Configure CMake (first time only)
cmake --preset=Debug

# Build
cmake --build build/Debug
```

## Debugging

### Method 1: Cortex-Debug Extension (Recommended)
1. Install the Cortex-Debug extension
2. Press `F5` or go to Run and Debug panel
3. Select "Debug (Cortex-Debug)" configuration
4. Click Start Debugging

This method automatically starts OpenOCD, flashes the device, and attaches the debugger.

### Method 2: Native cppdbg with OpenOCD
1. Start OpenOCD manually (or use the task):
   - Press `Ctrl+Shift+P` → "Tasks: Run Task" → "Start OpenOCD"
2. Press `F5` or go to Run and Debug panel
3. Select "Debug (OpenOCD)" configuration
4. Click Start Debugging

### Method 3: Command Line OpenOCD + GDB
```bash
# Terminal 1: Start OpenOCD
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg

# Terminal 2: Start GDB
arm-none-eabi-gdb build/Debug/basic-project.elf
(gdb) target extended-remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) monitor reset init
(gdb) continue
```

## Flashing Without Debugging

Use the Flash Device task:
```bash
# Via VS Code
Ctrl+Shift+P → Tasks: Run Task → Flash Device

# Via command line
openocd -f interface/stlink.cfg -f target/stm32h5x.cfg \
  -c "program build/Debug/basic-project.elf verify reset exit"
```

## Troubleshooting

### OpenOCD Can't Find Config Files
Make sure OpenOCD is installed and the config files exist:
```bash
openocd --search interface
openocd --search target
```

### Permission Denied for ST-Link
Add udev rules:
```bash
# Create udev rules file
sudo nano /etc/udev/rules.d/99-stlink.rules

# Add this line:
SUBSYSTEM=="usb", ATTRS{idVendor}=="0483", ATTRS{idProduct}=="374b", MODE="0666"

# Reload udev
sudo udevadm control --reload-rules
sudo udevadm trigger
```

### GDB Can't Connect
Ensure OpenOCD is running and listening on port 3333:
```bash
netstat -an | grep 3333
```

### Build Fails
Clean and reconfigure:
```bash
rm -rf build/Debug
cmake --preset=Debug
cmake --build build/Debug
```

## Hardware Connections

- **ST-Link V2/V3**: Connect SWDIO, SWCLK, GND, and optionally VCC
- **SWD Pins**: 
  - SWDIO: PA13
  - SWCLK: PA14
  - SWO (optional): PB3

## Project Structure

```
.
├── .vscode/               # VS Code configuration
│   ├── launch.json       # Debug configurations
│   ├── tasks.json        # Build and OpenOCD tasks
│   ├── settings.json     # Workspace settings
│   └── c_cpp_properties.json  # IntelliSense config
├── Core/                 # Application code
│   ├── Inc/              # Headers
│   └── Src/              # Source files
├── Drivers/              # HAL drivers
├── build/                # Build output
├── CMakeLists.txt        # CMake configuration
├── openocd.cfg           # OpenOCD configuration
└── README_DEBUG.md       # This file
```

## Additional Resources

- [OpenOCD Documentation](http://openocd.org/documentation/)
- [Cortex-Debug Extension](https://github.com/Marus/cortex-debug)
- [ARM GCC Documentation](https://gcc.gnu.org/onlinedocs/)
