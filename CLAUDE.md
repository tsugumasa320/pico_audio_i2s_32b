# CLAUDE.md
全て日本語で回答して下さい
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a 32-bit I2S DAC library for Raspberry Pi Pico/Pico 2, supporting stereo audio output up to 192 KHz sampling frequency. The library uses PIO (Programmable I/O) to implement I2S audio interface.

## Build Commands

### Prerequisites
- Set environment variables: `PICO_SDK_PATH`, `PICO_EXTRAS_PATH` (PICO_EXAMPLES_PATH not required)
- Confirmed with pico-sdk 2.1.1
- Note: pico-examples/ and pico-extras/ directories have been removed from the repository

### Windows (Developer Command Prompt for VS 2022)
```bash
cd samples/xxxxx  # sample project directory
mkdir build && cd build
cmake -G "NMake Makefiles" ..                                                      # Pico 1
cmake -G "NMake Makefiles" -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..          # Pico 2
nmake
```

### Linux
```bash
cd samples/xxxxx  # sample project directory
mkdir build && cd build
cmake ..                                                    # Pico 1
cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..        # Pico 2
make -j4
```

## Code Architecture

### Core Components

#### Main Library (`src/`)
- `audio_i2s.c` - Main I2S implementation using PIO and DMA
- `audio_i2s.pio` - PIO assembly program for I2S timing
- `include/pico/audio_i2s.h` - Public API definitions

#### Audio Processing (`src/pico_audio_32b/`)
- `audio.cpp` - Audio buffer management and format conversion
- `audio_utils.S` - Assembly utilities for audio processing
- `include/pico/audio.h` - Audio framework definitions
- `include/pico/sample_conversion.h` - Sample format conversion utilities

#### Configuration Macros (audio_i2s.h)
- `PICO_AUDIO_I2S_DATA_PIN` (default: 18) - I2S data pin
- `PICO_AUDIO_I2S_CLOCK_PIN_BASE` (default: 16) - I2S clock pins base
- `PICO_AUDIO_I2S_PIO` (default: 0) - PIO instance to use
- `PICO_AUDIO_I2S_DMA_IRQ` (default: 0) - DMA IRQ channel
- `CORE1_PROCESS_I2S_CALLBACK` - Enable dual-core processing

### Key Features
- Supports 16-bit and 32-bit PCM formats
- Stereo audio (mono-to-stereo conversion available)
- Dynamic frequency adjustment
- Double-buffered DMA for continuous playback
- Optional dual-core processing with Core1 handling callbacks

### Sample Projects
Located in `samples/` directory with individual CMakeLists.txt and README files showing usage patterns.

## Hardware Support
- Raspberry Pi Pico/Pico 2
- PCM5102 32-bit I2S DAC
- ES9023 24-bit I2S DAC

## Pin Mapping (Default)
- GP16: BCK (bit clock)
- GP17: LRCK (left/right clock)
- GP18: SDO (serial data output)
- VBUS: 5V power for DAC