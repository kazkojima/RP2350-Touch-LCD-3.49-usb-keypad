set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
set(PICO_BOARD waveshare_rp2350_touch_lcd_3.49 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(RP2350-Touch-LCD-3.49-LVGL C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

# Add compilation subdirectory
#add_subdirectory(./examples)
add_subdirectory(./lib/Config)
add_subdirectory(./lib/QSPI_PIO)
add_subdirectory(./lib/LCD)
add_subdirectory(./lib/Touch)
add_subdirectory(./lib/QMI8658)
add_subdirectory(./lib/PCF85063A)
add_subdirectory(./lib/lvgl)
add_subdirectory(./lib/PSRAM)

# Add header file directory
#include_directories(./examples)
include_directories(./lib/Config)
include_directories(./lib/QSPI_PIO)
include_directories(./lib/LCD)
include_directories(./lib/Touch)
include_directories(./lib/QMI8658)
include_directories(./lib/PCF85063A)
include_directories(./lib/lvgl)
include_directories(./lib/PSRAM)
include_directories(./examples/inc)

# PICO_CONFIG: PICO_XOSC_STARTUP_DELAY_MULTIPLIER, Multiplier to lengthen xosc startup delay to accommodate slow-starting oscillators, type=int, min=1, default=1, group=hardware_xosc

add_executable(RP2350-Touch-LCD-3.49-LVGL main.c keyboard.c lcd_3in49_lvgl_init.c usb_descriptors.c)

pico_generate_pio_header(RP2350-Touch-LCD-3.49-LVGL ${CMAKE_CURRENT_LIST_DIR}/lib/QSPI_PIO/qspi.pio
                        OUTPUT_DIR ${CMAKE_CURRENT_LIST_DIR}/lib/QSPI_PIO)


pico_set_program_name(RP2350-Touch-LCD-3.49-LVGL "RP2350-Touch-LCD-3.49-LVGL")
pico_set_program_version(RP2350-Touch-LCD-3.49-LVGL "0.1")

pico_enable_stdio_uart(RP2350-Touch-LCD-3.49-LVGL 1)
pico_enable_stdio_usb(RP2350-Touch-LCD-3.49-LVGL 0)

# Add the standard library to the build

set(LV_CONF_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/lib/LVGL/lv_conf.h
    CACHE STRING "" FORCE)
add_compile_options(-O2)

target_compile_definitions(RP2350-Touch-LCD-3.49-LVGL PRIVATE
    AUDIO_FREQ_MAX=48000

    # ours are zero based, so say so
    PICO_USBDEV_USE_ZERO_BASED_INTERFACES=1

    # need large descriptor
    PICO_USBDEV_MAX_DESCRIPTOR_SIZE=256

    PICO_USBDEV_ISOCHRONOUS_BUFFER_STRIDE_TYPE=1
    PICO_USBDEV_ENABLE_DEBUG_TRAgCE

)

target_link_libraries(RP2350-Touch-LCD-3.49-LVGL
                      Config
                      QSPI_PIO
                      LCD
                      Touch
                      PSRAM
                      QMI8658
                      PCF85063A
                      LVGL
                      pico_stdlib
                      hardware_pio
                      hardware_pwm 
                      hardware_dma
                      hardware_spi
                      hardware_i2c
                      hardware_gpio
                      hardware_adc
		      tinyusb_device
		      tinyusb_board
                      pico_multicore
)

# Default binaries to Copy code to RAM when booting from flash.
# Code execution speed may improve, but code size is limited by RAM capacity.
#pico_set_binary_type(RP2350-Touch-LCD-3.49-LVGL copy_to_ram)

# Add the standard include files to the build
target_include_directories(RP2350-Touch-LCD-3.49-LVGL PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(RP2350-Touch-LCD-3.49-LVGL)
