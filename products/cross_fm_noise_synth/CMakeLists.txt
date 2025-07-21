cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Pico SDK initialization
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
# pico-extras is required for pico_util_buffer dependency
set(PICO_EXTRAS_PATH ${CMAKE_CURRENT_LIST_DIR}/../../libs/pico-extras)
include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

# Cross FM Noise Synthesizer
project(cross_fm_noise_synth C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# Add DaisySP library
add_subdirectory(../../libs/DaisySP DaisySP)
add_subdirectory(../../libs/pico_audio_32b pico_audio_32b)

# Add pico_audio_core library target
if (NOT TARGET pico_audio_i2s_32b)
    add_library(pico_audio_i2s_32b INTERFACE)

    pico_generate_pio_header(pico_audio_i2s_32b
        ${CMAKE_CURRENT_LIST_DIR}/../../libs/pico_audio_core/audio_i2s.pio
    )

    target_sources(pico_audio_i2s_32b INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/../../libs/pico_audio_core/audio_i2s.c
    )

    target_link_libraries(pico_audio_i2s_32b INTERFACE
        pico_stdlib
        pico_multicore
        hardware_dma
        hardware_pio
        hardware_irq
        pico_audio_32b
    )

    target_include_directories(pico_audio_i2s_32b INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/../../libs/pico_audio_core/include
    )
endif()

# Add executable
add_executable(cross_fm_noise_synth
    src/main.cpp
    src/biquad_rbj.cpp
)

# Include directories
target_include_directories(cross_fm_noise_synth PRIVATE
    include
    ../../libs/DaisySP
)

# Link libraries
target_link_libraries(cross_fm_noise_synth
    pico_stdlib
    pico_audio_i2s_32b          # I2S audio output
    pico_audio_32b              # 32bit audio processing
    pico_util_buffer            # Required by pico_audio_32b
    DaisySP                     # DSP library
    hardware_spi                # For display/controls
    hardware_adc                # For analog controls
    hardware_pwm                # For LED indicators
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(cross_fm_noise_synth)

# Enable USB output (for debugging)
pico_enable_stdio_usb(cross_fm_noise_synth 1)
pico_enable_stdio_uart(cross_fm_noise_synth 0)