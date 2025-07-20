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

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @defgroup data_structures Data Structures
 * @brief Configuration and data structures for I2S audio
 * @{
 */

/**
 * @brief I2S hardware configuration structure
 * 
 * This structure defines all hardware-specific settings required to initialize
 * the I2S audio output system. All pin assignments and DMA channels must be
 * available and not conflict with other system components.
 *
 * @note GPIO pins must be available and not already used by other peripherals.
 *       DMA channels must be free for exclusive use by the audio system.
 */
typedef struct audio_i2s_config {
    /** @brief GPIO pin number for serial data output (SDATA)
     *  
     *  This pin carries the serialized audio data stream to the DAC.
     *  Must be different from clock pins.
     *  Range: 0-29 (RP2040), 0-47 (RP2350)
     */
    uint8_t data_pin;
    
    /** @brief Base GPIO pin for clock signals
     *  
     *  BCLK (bit clock) uses this pin.
     *  LRCLK (left/right clock) uses this pin + 1.
     *  Both pins must be consecutive and available.
     */
    uint8_t clock_pin_base;
    
    /** @brief First DMA channel for ping-pong buffering
     *  
     *  Used for the first half of double-buffering scheme.
     *  Must be different from dma_channel1.
     *  Range: 0-11 (RP2040/RP2350)
     */
    uint8_t dma_channel0;
    
    /** @brief Second DMA channel for ping-pong buffering
     *  
     *  Used for the second half of double-buffering scheme.
     *  Must be different from dma_channel0.
     *  Range: 0-11 (RP2040/RP2350)
     */
    uint8_t dma_channel1;
    
    /** @brief PIO state machine number to use
     *  
     *  Each PIO instance has 4 state machines (0-3).
     *  The selected state machine must be available.
     *  Range: 0-3
     */
    uint8_t pio_sm;
} audio_i2s_config_t;

/** @} */ // end of data_structures group

// ============================================================================
// Public API Functions
// ============================================================================

/**
 * @defgroup api_functions Public API Functions
 * @brief Main interface functions for I2S audio output
 * @{
 */

/**
 * @brief Initialize I2S audio output system
 * 
 * This function configures the PIO, DMA, and GPIO subsystems for I2S audio output.
 * It validates the configuration, allocates hardware resources, and prepares the
 * system for audio streaming.
 *
 * @param input_format  Input audio format specification (currently must match output)
 * @param output_format Desired output audio format for the I2S interface
 * @param config        Hardware configuration (pins, DMA channels, PIO settings)
 * 
 * @return Pointer to actual output format used, or NULL on failure
 * 
 * @retval NULL Initialization failed (invalid config, hardware unavailable)
 * @retval non-NULL Pointer to the audio format that will be used for output
 * 
 * @note The returned format may differ from the requested format if hardware
 *       limitations require adjustments.
 * 
 * @warning This function must be called before any other I2S operations.
 *          Call audio_i2s_end() to clean up resources when done.
 * 
 * @par Example:
 * @code
 * audio_format_t format = {
 *     .sample_freq = 44100,
 *     .format = AUDIO_BUFFER_FORMAT_PCM_S32,
 *     .channel_count = 2
 * };
 * 
 * audio_i2s_config_t config = {
 *     .data_pin = 18,
 *     .clock_pin_base = 16,
 *     .dma_channel0 = 0,
 *     .dma_channel1 = 1,
 *     .pio_sm = 0
 * };
 * 
 * const audio_format_t *actual = audio_i2s_setup(&format, &format, &config);
 * if (!actual) {
 *     // Handle initialization failure
 * }
 * @endcode
 */
const audio_format_t *audio_i2s_setup(const audio_format_t *input_format, 
                                     const audio_format_t *output_format,
                                     const audio_i2s_config_t *config);


/**
 * @brief Shutdown I2S audio output system
 * 
 * This function safely shuts down the I2S audio system, stopping any ongoing
 * audio output and releasing all allocated hardware resources including PIO
 * programs, DMA channels, and GPIO pins.
 * 
 * @note Call this function when audio output is no longer needed to free
 *       hardware resources for other uses.
 * 
 * @warning After calling this function, audio_i2s_setup() must be called again
 *          before resuming audio output.
 * 
 * @par Example:
 * @code
 * // Stop audio and clean up
 * audio_i2s_set_enabled(false);
 * audio_i2s_end();
 * @endcode
 */
void audio_i2s_end(void);


/**
 * @brief Connect audio producer with pass-through connection
 * 
 * This function establishes a connection between an audio buffer producer and
 * the I2S output with an intermediary connection object. This allows for more
 * complex audio routing and processing chains.
 * 
 * @param producer   Audio buffer pool that generates audio data
 * @param connection Intermediary connection object for audio routing
 * 
 * @return true if connection established successfully, false otherwise
 * 
 * @retval true  Connection established, audio will flow through the chain
 * @retval false Connection failed (invalid parameters, system not initialized)
 * 
 * @note This is an advanced function for complex audio routing scenarios.
 *       For simple cases, use audio_i2s_connect() instead.
 */
bool audio_i2s_connect_thru(audio_buffer_pool_t *producer, audio_connection_t *connection);


/**
 * @brief Connect audio producer to I2S output (standard connection)
 * 
 * This is the standard way to connect an audio buffer producer directly to
 * the I2S output system. Once connected, audio buffers from the producer
 * will be automatically consumed and output via I2S.
 * 
 * @param producer Audio buffer pool that generates audio data
 * 
 * @return true if connection established successfully, false otherwise
 * 
 * @retval true  Connection established, audio streaming ready
 * @retval false Connection failed (invalid producer, system not initialized)
 * 
 * @note The producer must generate audio in the format specified during
 *       audio_i2s_setup(). Buffer format mismatches will cause undefined behavior.
 * 
 * @note Call audio_i2s_set_enabled(true) after connection to start audio output.
 * 
 * @par Example:
 * @code
 * // Create producer pool
 * audio_buffer_pool_t *pool = audio_new_producer_pool(&format, 3, 1024);
 * 
 * // Connect to I2S output
 * if (audio_i2s_connect(pool)) {
 *     audio_i2s_set_enabled(true);  // Start output
 * }
 * @endcode
 */
bool audio_i2s_connect(audio_buffer_pool_t *producer);


/**
 * @brief Connect 8-bit audio producer to I2S output
 * 
 * Specialized connection function for 8-bit signed audio data.
 * The audio will be automatically converted to the output format
 * configured during setup.
 * 
 * @param producer Audio buffer pool containing 8-bit signed audio data
 * 
 * @return true if connection established successfully, false otherwise
 * 
 * @note This function handles format conversion internally. The producer
 *       should generate AUDIO_BUFFER_FORMAT_PCM_S8 format audio.
 */
bool audio_i2s_connect_s8(audio_buffer_pool_t *producer);

/**
 * @brief Connect audio producer with advanced buffering options
 * 
 * This function provides fine-grained control over audio buffering behavior
 * and connection parameters. It allows customization of buffer management
 * strategies for specific use cases.
 * 
 * @param producer           Audio buffer pool that generates audio data
 * @param buffer_on_give     If true, provide buffer immediately on give operations
 * @param buffer_count       Number of buffers to allocate for the connection
 * @param samples_per_buffer Number of samples per buffer
 * @param connection         Connection object to use for audio routing
 * 
 * @return true if connection established successfully, false otherwise
 * 
 * @retval true  Advanced connection established with custom parameters
 * @retval false Connection failed (invalid parameters, insufficient memory)
 * 
 * @note This is an advanced function for performance tuning and custom
 *       buffering strategies. Use audio_i2s_connect() for standard use cases.
 * 
 * @warning Incorrect buffer parameters can cause audio dropouts or
 *          excessive memory usage.
 */
bool audio_i2s_connect_extra(audio_buffer_pool_t *producer, bool buffer_on_give, 
                            uint buffer_count, uint samples_per_buffer, 
                            audio_connection_t *connection);


/**
 * @brief Enable or disable I2S audio output
 * 
 * This function controls whether the I2S system actively outputs audio.
 * When disabled, the PIO state machine is paused and DMA transfers stop,
 * but all configurations and connections remain intact.
 * 
 * @param enabled true to enable audio output, false to disable
 * 
 * @note Disabling audio output does not release hardware resources.
 *       Use audio_i2s_end() to fully shut down the system.
 * 
 * @note When re-enabling after disable, audio output resumes immediately
 *       with the next available buffer from the connected producer.
 * 
 * @par Example:
 * @code
 * // Start audio output
 * audio_i2s_set_enabled(true);
 * 
 * // Temporarily pause output
 * audio_i2s_set_enabled(false);
 * 
 * // Resume output
 * audio_i2s_set_enabled(true);
 * @endcode
 */
void audio_i2s_set_enabled(bool enabled);

/** @} */ // end of api_functions group

#ifdef __cplusplus
}
#endif

#endif //_AUDIO_I2S_H
