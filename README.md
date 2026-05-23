# RP2350-Touch-LCD-3.49-LVGL example: usb tenkey emulator

(This README is Assisted-by GEMINI CODE ASSIST)

This repository contains a tiny example project that turns [Waveshare RP2350-Touch-LCD-3.49](https://www.waveshare.com/wiki/RP2350-Touch-LCD-3.49) which is an RP2350-based development board with a 3.49-inch touch LCD into a USB HID numeric keypad (tenkey).

<img src="https://github.com/kazkojima/RP2350-Touch-LCD-3.49-usb-keypad/blob/junkyard/images/tenkey-emu.png" alt="Testing tenkey example" width="640">

It uses the **LVGL (Light and Versatile Graphics Library)** for the user interface and the **Pico SDK**'s USB stack (TinyUSB) for keyboard emulation.

The LCD on Waveshare RP2350-Touch-LCD-3.49 is portrait-oriented and the LCD controller AXS15231B does not support hardware-based screen rotation. This means that software-based rotation is required for landscape-oriented applications like this example.

The USB part is based on [the HID device example of pico-example](https://github.com/raspberrypi/pico-examples/tree/master/usb/device/dev_hid_composite). The files in lib directory except in the LVGL-9.5 tree come from [LVGL Demo in https://www.waveshare.com/wiki/RP2350-Touch-LCD-3.49](https://files.waveshare.com/wiki/RP2350-Touch-LCD-3.49/RP2350-Touch-LCD-3.49-LVGL.zip).

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

## Hints for Configuration

### Screen orientation
The display uses software rotation. You can adjust the rotation angle within lcd_3in49_lvgl_init.c:disp_flush_cb function if your hardware mounting requires a different orientation.

### Quiet mode
If there is no key activity for 60 seconds, the device will enter quiet mode. In quiet mode, the screen goes dark and no key action sends to the host. Press the power button to wake it up. This behavior is controlled with:
```
#define ENABLE_QUIET_MODE 1
#define QUIET_AFTER 60
```
in main.c.

## Experimental Features

- **Macro key**: The macro key function acts as a simple "physical key" emulator. It combines data stored on an SD card with secret keys stored in the microcontroller's flash memory to "type" a pre-defined sequence of characters over USB.  It works in the steps below:

  1. Trigger and Authentication
    - Activation: The process begins when the user clicks the macro key widget (represented by LV_SYMBOL_UPLOAD) on the touch screen.
    - PIN Entry: If ENABLE_PIN_KEY is enabled, the system prompts the user for a 4-digit PIN via the on-screen numeric keypad. This PIN is not just a password; it is used as a bit-offset (pin_value) to select a specific starting point within the secret key data stored in flash.
  2. Data Retrieval and Decryption
    - Physical Token (SD Card): The system attempts to read Sector 0 (the MBR) of an inserted SD card. It targets a specific offset (SECTOR_DATA_OFS, which is 32) to find the encrypted macro data.
    - The "Secret" (Flash Memory): It references a section of the RP2350's flash memory labeled __device_key__.
    - XOR Transformation: The function retrieves 32 bytes from the SD card and XORs them with bytes derived from the flash memory. The specific bits pulled from flash are determined by the user-entered PIN.
    - Result: The resulting macro_codes buffer contains the raw ASCII sequence to be typed. The first byte defines the length of the macro.
  3. USB HID Emulation
    - Typing Task: Once the data is decrypted, the macro_mode flag is set. The hid_task (running in the main loop) iterates through the macro_codes buffer.
    - Conversion: Each ASCII character is converted to a USB HID keycode using a conversion table (conv_table).
    - Transmission: The characters are sent sequentially to the host PC as keyboard reports via the TinyUSB stack, effectively "typing" the password or command string.
  4. Security and Cleanup
    - One-Time Use: If ENABLE_MACRO_ONETIME is set, the SD card is programmatically deselected after one execution to prevent immediate re-use.
    - Memory Wiping: After the macro finishes typing, the macro_codes and secbuf (the SD sector buffer) are explicitly zeroed out using memset to prevent sensitive data from lingering in RAM.

  For setup, an external application prepares 4096 random bytes which will be written to the flash page and generates a key which will be written into the SD card based on the user-specified plain text and pin-value. Here is a pseudo code of that application:

```
  uint8_t randoms[4096]; // Read from /dev/random, for example.
  size_t bit_ofs = user_specified_PIN_value; // <= 9999
  uint8_t user_text_with_length[32]; // = { 6, 'H', 'e', 'l', 'l', 'o', '\n', };
  uint8_t key_on_sdcard[32];
  for (int i=0; i < 32; i++) {
    size_t byte_ofs = i + (bit_ofs >> 3);
    size_t ofs_in_byte = bit_ofs & 7;
    uint8_t key;
    key = randoms[byte_ofs] >> ofs_in_byte) | (randoms[byte_ofs+1] << (8-ofs_in_byte));
    key_on_sdcard[i] = user_text_with_length[i] ^ key;
  }
  // write randoms to a file
  // write key_on_sdcard to a file
```
