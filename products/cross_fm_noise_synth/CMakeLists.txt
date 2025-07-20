cmake_minimum_required(VERSION 3.13)

# Cross FM Noise Synthesizer
project(cross_fm_noise_synth)

# Pico SDK initialization
include(../../pico_sdk_import.cmake)
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
    src/fm_engine.cpp
    src/noise_generator.cpp
    src/cross_modulator.cpp
    src/ui_controller.cpp
    src/preset_manager.cpp
    src/analog_multiplexer.c
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