cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
# pico-extras is required for pico_util_buffer dependency
if(DEFINED ENV{PICO_EXTRAS_PATH})
    include($ENV{PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
else()
    # Try local path
    set(PICO_EXTRAS_PATH ${CMAKE_CURRENT_LIST_DIR}/../../libs/pico-extras)
    include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
endif()

set(project_name "sine_wave_i2s_32b" C CXX ASM)
project(${project_name})
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(../../libs/pico_audio_32b pico_audio_32b)
add_subdirectory(../.. pico_audio_i2s_32b)

set(bin_name ${PROJECT_NAME})
add_executable(${PROJECT_NAME}
    sine_wave.cpp
)

pico_enable_stdio_usb(${bin_name} 1)
pico_enable_stdio_uart(${bin_name} 1)

target_link_libraries(${bin_name} PRIVATE
    pico_stdlib
    pico_audio_32b
    pico_audio_i2s_32b
    pico_util_buffer
    hardware_adc
)

# set PIO and DMA for I2S
# set core1 process i2s_callback
#target_compile_definitions(${PROJECT_NAME} PRIVATE
#    PICO_AUDIO_I2S_PIO=1
#    PICO_AUDIO_I2S_DMA_IRQ=1
#    CORE1_PROCESS_I2S_CALLBACK
#)

pico_add_extra_outputs(${bin_name})
