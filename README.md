# RP2350-Touch-LCD-3.49-LVGL example: usb tenkey emulator

(This README is Assisted-by GEMINI CODE ASSIST)

This repository contains a tiny example project that turns Waveshare RP2350-Touch-LCD-3.49 which is an RP2350-based development board with a 3.49-inch touch LCD into a USB HID numeric keypad (tenkey).

<img src="https://github.com/kazkojima/RP2350-Touch-LCD-3.49-usb-keypad/blob/junkyard/images/tenkey-emu.png" alt="Testing tenkey example" width="640">

It uses the **LVGL (Light and Versatile Graphics Library)** for the user interface and the **Pico SDK**'s USB stack (TinyUSB) for keyboard emulation.

The LCD on Waveshare RP2350-Touch-LCD-3.49 is portrait-oriented and the LCD controller AXS15231B does not support hardware-based screen rotation. This means that software-based rotation is required for landscape-oriented applications like this example.

## Key Features

- **USB HID Keyboard Emulation**: Acts as a plug-and-play numeric keypad when connected to a PC.
- **Touch-Based Input**: Interactive numeric buttons rendered on the LCD.
- **Software Display Rotation**: This example implements software-based rotation at the whole display level to support different hardware orientations.
- **Multicore Architecture**:
  - **Core 0**: Handles the USB HID stack (TinyUSB) and HID report tasks.
  - **Core 1**: Dedicated to LVGL rendering and touch event processing for a smooth UI.
- **Action Keys**: Includes dedicated touch buttons for Backspace, Enter, Arrow Up, and Arrow Down.

## Hardware Requirements

- **Microcontroller**: Raspberry Pi RP2350.
- **Display**: Waveshare RP2350-Touch-LCD-3.49.
- **Connection**: USB-C cable for both power and HID data transmission.

## Dependencies

This project requires the following to be set up in your environment:

- **Raspberry Pi Pico SDK**: v2.0.0 or later (required for RP2350 support).
- **LVGL**: Included as a submodule or library dependency.
- **picotool**: For flashing and inspecting binaries.

## Building the Project

1. Initialize the Pico SDK and environment variables:
   ```bash
   export PICO_SDK_PATH=/path/to/pico-sdk
   ```

2. Create a build directory and run CMake:
   ```bash
   mkdir build
   cd build
   cmake ..
   make -j
   ```

## Installation

### Using UF2 (Drag and Drop)
1. Push and hold the **BOOT** button on your RP2350 board.
2. Connect the board to your computer via USB.
3. Drag and drop the `RP2350-Touch-LCD-3.49-LVGL.uf2` file into the `RP2350` mass storage device.

### Using picotool
If you have `picotool` installed, you can flash the device directly:
```bash
picotool load RP2350-Touch-LCD-3.49-LVGL.uf2 -x
```

## UI Configuration
The display uses software rotation. You can adjust the rotation angle within lcd_3in49_lvgl_init.c:disp_flush_cb function if your hardware mounting requires a different orientation.
