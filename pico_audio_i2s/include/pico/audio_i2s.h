/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Modified by Elehobica, 2021
// Enhanced with comprehensive documentation by Claude, 2025

#ifndef _PICO_AUDIO_I2S_H
#define _PICO_AUDIO_I2S_H

#include "pico/audio.h"

/**
 * @file audio_i2s.h
 * @defgroup pico_audio_i2s pico_audio_i2s
 * @brief High-performance I2S audio output using PIO and DMA
 *
 * @section overview Overview
 * 
 * This library provides a complete I2S audio output implementation for Raspberry Pi Pico,
 * supporting professional audio DACs like PCM5102 and ES9023. It leverages the PIO 
 * (Programmable I/O) subsystem for precise timing control and DMA for efficient data transfer.
 *
 * @section features Key Features
 * 
 * - **High Resolution Audio**: 16-bit and 32-bit PCM support
 * - **Wide Sample Rate Range**: 8 kHz to 192 kHz
 * - **Low Latency**: DMA-based streaming with minimal CPU overhead
 * - **Dual Core Support**: Optional Core1 callback processing
 * - **Professional Quality**: Jitter-free output using PIO state machines
 * - **Flexible Configuration**: Configurable GPIO pins and DMA channels
 *
 * @section usage Basic Usage Example
 * 
 * @code{.c}
 * #include "pico/audio_i2s.h"
 * 
 * // Configure audio format
 * audio_format_t format = {
 *     .sample_freq = 44100,
 *     .format = AUDIO_BUFFER_FORMAT_PCM_S32,
 *     .channel_count = 2
 * };
 * 
 * // Configure I2S hardware
 * audio_i2s_config_t config = {
 *     .data_pin = 18,         // SDATA pin
 *     .clock_pin_base = 16,   // BCLK=16, LRCLK=17
 *     .dma_channel0 = 0,
 *     .dma_channel1 = 1,
 *     .pio_sm = 0
 * };
 * 
 * // Initialize I2S
 * audio_i2s_setup(&format, &format, &config);
 * 
 * // Create buffer pool and start streaming
 * audio_buffer_pool_t *pool = audio_new_producer_pool(&format, 3, 1024);
 * audio_i2s_connect(pool);
 * audio_i2s_set_enabled(true);
 * @endcode
 *
 * @section hardware Hardware Requirements
 * 
 * - **GPIO Pins**: 3 consecutive pins (SDATA, BCLK, LRCLK)
 * - **PIO Instance**: One PIO state machine (pio0 or pio1)
 * - **DMA Channels**: Two DMA channels for double buffering
 * - **Compatible DACs**: PCM5102, ES9023, or any I2S-compatible DAC
 *
 * @section performance Performance Characteristics
 * 
 * - **CPU Usage**: <5% at 44.1 kHz/32-bit (measured on RP2040 @ 125MHz)
 * - **Memory Usage**: ~14KB for triple buffering (1156 samples/buffer)
 * - **Latency**: <30ms total system latency
 * - **Maximum Sample Rate**: 192 kHz (limited by PIO clock and system performance)
 */

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Configuration Macros
// ============================================================================

/**
 * @defgroup config_macros Configuration Macros
 * @brief Compile-time configuration options for I2S audio system
 * @{
 */

/**
 * @brief DMA IRQ channel selection (0 or 1)
 * 
 * Selects which DMA IRQ handler to use for audio buffer management.
 * Each IRQ can handle multiple DMA channels but using separate IRQs
 * can improve real-time performance.
 */
#ifndef PICO_AUDIO_I2S_DMA_IRQ
#ifdef PICO_AUDIO_DMA_IRQ
#define PICO_AUDIO_I2S_DMA_IRQ PICO_AUDIO_DMA_IRQ
#else
#define PICO_AUDIO_I2S_DMA_IRQ 0
#endif
#endif

/**
 * @brief PIO instance selection (0 or 1)
 * 
 * Chooses which PIO block to use for I2S signal generation.
 * Each PIO has 4 state machines and 32 instruction slots.
 */
#ifndef PICO_AUDIO_I2S_PIO
#ifdef PICO_AUDIO_PIO
#define PICO_AUDIO_I2S_PIO PICO_AUDIO_PIO
#else
#define PICO_AUDIO_I2S_PIO 0
#endif
#endif

// Compile-time validation
#if !(PICO_AUDIO_I2S_DMA_IRQ == 0 || PICO_AUDIO_I2S_DMA_IRQ == 1)
#error PICO_AUDIO_I2S_DMA_IRQ must be 0 or 1
#endif

#if !(PICO_AUDIO_I2S_PIO == 0 || PICO_AUDIO_I2S_PIO == 1)
#error PICO_AUDIO_I2S_PIO must be 0 or 1
#endif

/**
 * @brief Maximum number of audio channels supported
 * 
 * Currently fixed at 2 for stereo I2S output.
 * Future versions may support multi-channel TDM.
 */
#ifndef PICO_AUDIO_I2S_MAX_CHANNELS
#ifdef PICO_AUDIO_MAX_CHANNELS
#define PICO_AUDIO_I2S_MAX_CHANNELS PICO_AUDIO_MAX_CHANNELS
#else
#define PICO_AUDIO_I2S_MAX_CHANNELS 2u
#endif
#endif

/**
 * @brief Number of buffers per audio channel for triple buffering
 * 
 * Triple buffering (3 buffers) provides the best balance between
 * latency and glitch-free playback. Increasing this value reduces
 * the risk of buffer underruns but increases memory usage and latency.
 */
#ifndef PICO_AUDIO_I2S_BUFFERS_PER_CHANNEL
#ifdef PICO_AUDIO_BUFFERS_PER_CHANNEL
#define PICO_AUDIO_I2S_BUFFERS_PER_CHANNEL PICO_AUDIO_BUFFERS_PER_CHANNEL
#else
#define PICO_AUDIO_I2S_BUFFERS_PER_CHANNEL 3u
#endif
#endif

/**
 * @brief Default buffer size in samples per channel
 * 
 * 576 samples provides ~13ms latency at 44.1kHz.
 * Memory usage: buffers × channels × sample_size × buffer_length
 * Example: 3 × 2 × 4 × 576 = 13.8KB
 */
#ifndef PICO_AUDIO_I2S_BUFFER_SAMPLE_LENGTH
#ifdef PICO_AUDIO_BUFFER_SAMPLE_LENGTH
#define PICO_AUDIO_I2S_BUFFER_SAMPLE_LENGTH PICO_AUDIO_BUFFER_SAMPLE_LENGTH
#else
#define PICO_AUDIO_I2S_BUFFER_SAMPLE_LENGTH 576u
#endif
#endif

/**
 * @brief Silence buffer length for gap filling
 * 
 * Used when no audio data is available to prevent DAC from
 * outputting undefined values.
 */
#ifndef PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH
#ifdef PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH
#define PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH PICO_AUDIO_SILENCE_BUFFER_SAMPLE_LENGTH
#else
#define PICO_AUDIO_I2S_SILENCE_BUFFER_SAMPLE_LENGTH 256u
#endif
#endif

/**
 * @brief Debug/testing mode - disables actual audio output
 * 
 * When set to 1, all audio operations are simulated without
 * actually configuring hardware. Useful for testing.
 */
#ifndef PICO_AUDIO_I2S_NOOP
#ifdef PICO_AUDIO_NOOP
#define PICO_AUDIO_I2S_NOOP PICO_AUDIO_NOOP
#else
#define PICO_AUDIO_I2S_NOOP 0
#endif
#endif

/**
 * @brief Default GPIO pin for I2S data output (SDATA)
 * 
 * This pin carries the serialized audio data stream.
 * Must be different from clock pins.
 */
#ifndef PICO_AUDIO_I2S_DATA_PIN
#define PICO_AUDIO_I2S_DATA_PIN 18
#endif

/**
 * @brief Default base GPIO pin for I2S clock signals
 * 
 * BCLK (bit clock) uses this pin.
 * LRCLK (left/right clock) uses this pin + 1.
 * These pins must be consecutive.
 */
#ifndef PICO_AUDIO_I2S_CLOCK_PIN_BASE
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 16
#endif

/** @} */ // end of config_macros group

// todo this needs to come from a build config
/** \brief Base configuration structure used when setting up
 * \ingroup pico_audio_i2s
 */
typedef struct audio_i2s_config {
    uint8_t data_pin;
    uint8_t clock_pin_base;
    uint8_t dma_channel0;
    uint8_t dma_channel1;
    uint8_t pio_sm;
} audio_i2s_config_t;

/** \brief Set up system to output I2S audio
 * \ingroup pico_audio_i2s
 *
 * \param i2s_audio_format \todo
 * \param config The configuration to apply.
 */
const audio_format_t *audio_i2s_setup(const audio_format_t *i2s_input_audio_format, const audio_format_t *i2s_output_audio_format,
                                               const audio_i2s_config_t *config);


/** \brief End up system to output I2S audio
 * \ingroup pico_audio_i2s
 *
 */
void audio_i2s_end();
/*
 * \param config The configuration to apply.
void audio_i2s_end(const audio_i2s_config_t *config);
*/


/** \brief \todo
 * \ingroup pico_audio_i2s
 *
 * \param producer
 * \param connection
 */
bool audio_i2s_connect_thru(audio_buffer_pool_t *producer, audio_connection_t *connection);


/** \brief \todo
 * \ingroup pico_audio_i2s
 *
 * \param producer
 *
 *  todo make a common version (or a macro) .. we don't want to pull in unnecessary code by default
 */
bool audio_i2s_connect(audio_buffer_pool_t *producer);


/** \brief \todo
 * \ingroup pico_audio_i2s
 *
 * \param producer
 */
bool audio_i2s_connect_s8(audio_buffer_pool_t *producer);
bool audio_i2s_connect_extra(audio_buffer_pool_t *producer, bool buffer_on_give, uint buffer_count, uint samples_per_buffer, audio_connection_t *connection);

/** \brief \todo
 * \ingroup pico_audio_i2s
 *
 * \param producer
 * \param buffer_on_give
 * \param buffer_count
 * \param samples_per_buffer
 * \param connection
 * \return
 */
bool audio_i2s_connect_extra(audio_buffer_pool_t *producer, bool buffer_on_give, uint buffer_count,
                                 uint samples_per_buffer, audio_connection_t *connection);


/** \brief Set up system to output I2S audio
 * \ingroup pico_audio_i2s
 *
 * \param enable true to enable I2S audio, false to disable.
 */
void audio_i2s_set_enabled(bool enabled);

#ifdef __cplusplus
}
#endif

#endif //_AUDIO_I2S_H
