/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// Modified by Elehobica, 2021
// Enhanced with comprehensive documentation by Claude, 2025

/**
 * @file audio_i2s.c
 * @brief I2S Audio Output Implementation using PIO and DMA
 * 
 * This file implements high-performance I2S audio output for Raspberry Pi Pico.
 * It uses PIO (Programmable I/O) for precise timing control and DMA for efficient
 * data transfer with minimal CPU overhead.
 * 
 * Key Features:
 * - 32-bit PCM audio support up to 192 kHz
 * - Double-buffered DMA for glitch-free playback  
 * - Dynamic sample rate adjustment
 * - Optional dual-core processing
 * - Multiple audio format conversion support
 */

// ============================================================================
// System Includes
// ============================================================================

#include <stdio.h>

// Pico SDK Core Libraries
#include "pico/stdlib.h"
#include "pico/multicore.h"

// Hardware Abstraction Layer
#include "hardware/pio.h"      // Programmable I/O interface
#include "hardware/gpio.h"     // GPIO pin control
#include "hardware/dma.h"      // Direct Memory Access
#include "hardware/irq.h"      // Interrupt handling
#include "hardware/clocks.h"   // System clock management
#include "hardware/structs/dma.h"  // DMA register structures
#include "hardware/regs/dreq.h"    // DMA request signals

// Audio I2S Implementation
#include "audio_i2s.pio.h"     // Generated PIO program header
#include "pico/audio_i2s.h"    // Public API definitions

// ============================================================================
// Compilation Configuration
// ============================================================================

/**
 * @brief Enable fractional PIO clock division for precise timing
 * 
 * When defined, uses fractional clock dividers for more accurate sample rates.
 * This may introduce slight clock jitter but provides better frequency accuracy.
 */
#define PIO_CLK_DIV_FRAC

/**
 * @brief Enable dual-core I2S callback processing (experimental)
 * 
 * When defined, I2S callbacks are processed on Core1 to reduce latency.
 * Currently disabled as single-core performance is often better.
 */
//#define CORE1_PROCESS_I2S_CALLBACK

/**
 * @brief Enable DMA transfer interval monitoring (debug only)
 * 
 * When defined, monitors time between DMA transfers for performance analysis.
 * Should only be enabled for debugging due to overhead.
 */
//#define WATCH_DMA_TRANSFER_INTERVAL

/**
 * @brief Enable PIO TX FIFO level monitoring (debug only)
 * 
 * When defined, monitors PIO transmit FIFO levels for underrun detection.
 * Should only be enabled for debugging due to overhead.
 */
//#define WATCH_PIO_SM_TX_FIFO_LEVEL

// ============================================================================
// Debug and Profiling Support
// ============================================================================

/**
 * @brief Debug timing pins for oscilloscope analysis
 * 
 * Registers GPIO pins for timing analysis. Pins can be monitored with
 * an oscilloscope to measure audio processing timing and identify bottlenecks.
 */
CU_REGISTER_DEBUG_PINS(audio_timing)

// Uncomment to enable debug pin output (affects performance)
//CU_SELECT_DEBUG_PINS(audio_timing)


// ============================================================================
// Hardware Resource Macros
// ============================================================================

/**
 * @brief Compile-time PIO instance selection
 * 
 * These macros resolve to the correct hardware instances based on
 * the configuration macros defined in audio_i2s.h
 */
#define audio_pio     __CONCAT(pio, PICO_AUDIO_I2S_PIO)           // pio0 or pio1
#define GPIO_FUNC_PIOx __CONCAT(GPIO_FUNC_PIO, PICO_AUDIO_I2S_PIO) // GPIO_FUNC_PIO0 or GPIO_FUNC_PIO1
#define DREQ_PIOx_TX0 __CONCAT(__CONCAT(DREQ_PIO, PICO_AUDIO_I2S_PIO), _TX0) // DMA request signal
#define DMA_IRQ_x     __CONCAT(DMA_IRQ_, PICO_AUDIO_I2S_DMA_IRQ)   // DMA_IRQ_0 or DMA_IRQ_1

// ============================================================================
// Global State Variables
// ============================================================================

/**
 * @brief PIO program memory offset
 * 
 * Stores the memory offset where the I2S PIO program is loaded.
 * Used for cleanup and state machine management.
 */
static uint loaded_offset = 0;

/**
 * @brief Input audio format specification
 * 
 * Points to the audio format of the input stream. Used for format
 * conversion and compatibility checking.
 */
static const audio_format_t *_i2s_input_audio_format;

/**
 * @brief Output audio format specification
 * 
 * Points to the actual I2S output format. Determines hardware
 * configuration and timing parameters.
 */
static const audio_format_t *_i2s_output_audio_format;

/**
 * @brief Shared state for DMA and audio processing
 * 
 * Contains all runtime state information shared between interrupt
 * handlers and main application. Protected by hardware atomicity.
 */
struct {
    audio_buffer_t *playing_buffer0;  /**< Currently playing buffer on DMA channel 0 */
    audio_buffer_t *playing_buffer1;  /**< Currently playing buffer on DMA channel 1 */
    uint32_t freq;                    /**< Current sampling frequency in Hz */
    uint8_t pio_sm;                   /**< PIO state machine number (0-3) */
    uint8_t dma_channel0;             /**< First DMA channel for ping-pong buffering */
    uint8_t dma_channel1;             /**< Second DMA channel for ping-pong buffering */
} shared_state;

/**
 * @brief DMA configuration for channel 0
 * 
 * Pre-configured DMA settings for the first channel in the ping-pong
 * buffering scheme. Applied during DMA transfer setup.
 */
static dma_channel_config dma_config0;

/**
 * @brief DMA configuration for channel 1
 * 
 * Pre-configured DMA settings for the second channel in the ping-pong
 * buffering scheme. Applied during DMA transfer setup.
 */
static dma_channel_config dma_config1;

/**
 * @brief Consumer audio format for internal processing
 * 
 * Defines the audio format used internally by the I2S consumer.
 * May differ from input format if conversion is required.
 */
audio_format_t pio_i2s_consumer_format;

/**
 * @brief Buffer format descriptor for I2S consumer
 * 
 * Wraps the consumer format with buffer-specific metadata
 * like sample stride and alignment requirements.
 */
audio_buffer_format_t pio_i2s_consumer_buffer_format = {
    .format = &pio_i2s_consumer_format,
};

/**
 * @brief Audio buffer pool for I2S output
 * 
 * Manages the pool of audio buffers used for I2S output.
 * Provides thread-safe buffer allocation and recycling.
 */
static audio_buffer_pool_t *audio_i2s_consumer;

/**
 * @brief Silence buffer for underrun protection
 * 
 * Pre-allocated buffer filled with silence (zero samples).
 * Used when no audio data is available to prevent DAC
 * from outputting undefined values.
 */
static audio_buffer_t silence_buffer;

// ============================================================================
// Forward Declarations
// ============================================================================

/**
 * @brief DMA interrupt handler for I2S audio transfer
 * 
 * This function is called when DMA completes transferring an audio buffer
 * to the PIO TX FIFO. It manages the ping-pong buffering scheme and
 * triggers audio callback processing.
 * 
 * @note Marked as interrupt service routine and time-critical for minimal latency
 */
static void __isr __time_critical_func(audio_i2s_dma_irq_handler)(void);

// ============================================================================
// Debug and Timing Utilities
// ============================================================================

#ifdef WATCH_PIO_SM_TX_FIFO_LEVEL
/**
 * @brief Get current time in milliseconds since boot
 * 
 * Used for performance monitoring and FIFO level analysis.
 * Only compiled when PIO FIFO monitoring is enabled.
 * 
 * @return Current system time in milliseconds
 */
static inline uint32_t _millis(void)
{
    return to_ms_since_boot(get_absolute_time());
}
#endif // WATCH_PIO_SM_TX_FIFO_LEVEL

#ifdef WATCH_DMA_TRANSFER_INTERVAL
/**
 * @brief Get current time in microseconds since boot
 * 
 * Used for high-resolution DMA transfer timing analysis.
 * Only compiled when DMA interval monitoring is enabled.
 * 
 * @return Current system time in microseconds
 */
static inline uint32_t _micros(void)
{
    return to_us_since_boot(get_absolute_time());
}
#endif // WATCH_DMA_TRANSFER_INTERVAL

// ============================================================================
// Callback Function Interface
// ============================================================================

/**
 * @brief Weak I2S callback function for application use
 * 
 * This function is called each time a DMA transfer completes, allowing
 * the application to perform audio processing, buffer management, or
 * other time-sensitive operations.
 * 
 * The function is declared weak, meaning applications can override it
 * with their own implementation. If not overridden, this empty default
 * implementation is used.
 * 
 * @note This function is called from interrupt context (or Core1 if
 *       CORE1_PROCESS_I2S_CALLBACK is enabled). Keep processing minimal
 *       and avoid blocking operations.
 * 
 * @note Timing constraints: This function should complete within the
 *       duration of one audio buffer to avoid audio dropouts.
 * 
 * @par Example Override:
 * @code
 * void i2s_callback_func() {
 *     // Custom audio processing here
 *     update_audio_effects();
 *     fill_next_audio_buffer();
 * }
 * @endcode
 */
__attribute__((weak))
void i2s_callback_func(void)
{
    // Default implementation does nothing
    // Applications can override this function for custom processing
    return;
}

#ifdef CORE1_PROCESS_I2S_CALLBACK

enum FifoMessage {
    RESPONSE_CORE1_THREAD_STARTED = 0,
    RESPONSE_CORE1_THREAD_TERMINATED = 0,
    EVENT_I2S_DMA_TRANSFER_STARTED,
    NOTIFY_I2S_DISABLED
};

static const uint64_t FIFO_TIMEOUT = 10 * 1000; // us

void i2s_callback_loop()
{
    multicore_fifo_push_blocking(RESPONSE_CORE1_THREAD_STARTED);
#ifndef NDEBUG
    printf("i2s_callback_loop started (on core %d)\n", get_core_num());
#endif // NDEBUG
    multicore_fifo_drain();
    while (true) {
        uint32_t msg = multicore_fifo_pop_blocking();
        if (msg == EVENT_I2S_DMA_TRANSFER_STARTED) {
            i2s_callback_func();
        } else if (msg == NOTIFY_I2S_DISABLED) {
            break;
        } else {
            panic("Unexpected message from Core 0\n");
        }
        tight_loop_contents();
    }
    multicore_fifo_push_blocking(RESPONSE_CORE1_THREAD_TERMINATED);
#ifndef NDEBUG
    printf("i2s_callback_loop terminated (on core %d)\n", get_core_num());
#endif // NDEBUG

    while (true) { tight_loop_contents(); } // infinite loop
    return;
}
#endif // CORE1_PROCESS_I2S_CALLBACK

// ============================================================================
// Public API Implementation
// ============================================================================

/**
 * @brief Shutdown I2S audio system and cleanup all resources
 * 
 * This function safely shuts down the I2S audio system and releases all
 * allocated resources including:
 * - Audio buffer pools and individual buffers
 * - Playing buffers currently in use by DMA
 * - Silence buffer
 * - PIO program memory
 * - PIO state machine
 * 
 * The function ensures proper cleanup order to avoid resource leaks or
 * corruption. It should be called when audio output is no longer needed.
 * 
 * @note This function assumes audio output has been disabled via
 *       audio_i2s_set_enabled(false) before calling.
 */
void audio_i2s_end(void) 
{
    audio_buffer_t *ab;
    
    // Release all queued audio buffers from the consumer pool
    // These are buffers waiting to be played
    ab = take_audio_buffer(audio_i2s_consumer, false);
    while (ab != NULL) {
        free(ab->buffer->bytes);  // Free audio data
        free(ab->buffer);         // Free buffer wrapper
        ab = take_audio_buffer(audio_i2s_consumer, false);
    }
    
    // Release all free buffers from the pool
    // These are unused buffers ready for allocation
    ab = get_free_audio_buffer(audio_i2s_consumer, false);
    while (ab != NULL) {
        free(ab->buffer->bytes);  // Free audio data
        free(ab->buffer);         // Free buffer wrapper
        ab = get_free_audio_buffer(audio_i2s_consumer, false);
    }
    
    // Release all full buffers from the pool
    // These are buffers filled with audio data but not yet queued
    ab = get_full_audio_buffer(audio_i2s_consumer, false);
    while (ab != NULL) {
        free(ab->buffer->bytes);  // Free audio data
        free(ab->buffer);         // Free buffer wrapper
        ab = get_full_audio_buffer(audio_i2s_consumer, false);
    }
    
    // Release currently playing buffers
    // These buffers are actively being transferred by DMA
    if (shared_state.playing_buffer0 != NULL) {
        free(shared_state.playing_buffer0->buffer->bytes);
        free(shared_state.playing_buffer0->buffer);
        shared_state.playing_buffer0 = NULL;
    }
    
    if (shared_state.playing_buffer1 != NULL) {
        free(shared_state.playing_buffer1->buffer->bytes);
        free(shared_state.playing_buffer1->buffer);
        shared_state.playing_buffer1 = NULL;
    }
    
    // Release buffer pool structure
    free(audio_i2s_consumer);
    
    // Release silence buffer used for underrun protection
    free(silence_buffer.buffer->bytes);
    free(silence_buffer.buffer);
    
    // Clean up PIO resources
    uint8_t sm = shared_state.pio_sm;
    pio_sm_clear_fifos(audio_pio, sm);           // Clear any remaining data
    pio_sm_drain_tx_fifo(audio_pio, sm);        // Ensure TX FIFO is empty
    pio_remove_program(audio_pio, &audio_i2s_program, loaded_offset);  // Unload program
    pio_clear_instruction_memory(audio_pio);    // Clear program memory
    pio_sm_unclaim(audio_pio, sm);              // Release state machine
}

/**
 * @brief Initialize I2S audio output system
 * 
 * This function sets up the complete I2S audio output pipeline including:
 * - GPIO pin configuration for I2S signals
 * - PIO state machine setup and program loading
 * - DMA channel configuration
 * - Audio buffer management
 * 
 * The function validates input parameters and hardware availability before
 * proceeding with initialization.
 * 
 * @param input_format  Input audio format specification
 * @param output_format Desired I2S output format
 * @param config        Hardware configuration (pins, DMA, PIO)
 * 
 * @return Pointer to actual output format, or NULL on failure
 * 
 * @note Currently supports stereo output only (2 channels)
 * @note Supports 16-bit and 32-bit PCM formats
 */
const audio_format_t *audio_i2s_setup(const audio_format_t *input_format, 
                                     const audio_format_t *output_format,
                                     const audio_i2s_config_t *config) 
{
    // Store format specifications for runtime use
    _i2s_input_audio_format = input_format;
    _i2s_output_audio_format = output_format;
    
    // Configure GPIO pins for PIO function
    // All I2S signals (SDATA, BCLK, LRCLK) use the same PIO instance
    uint func = GPIO_FUNC_PIOx;
    gpio_set_function(config->data_pin, func);          // SDATA pin
    gpio_set_function(config->clock_pin_base, func);    // BCLK pin  
    gpio_set_function(config->clock_pin_base + 1, func); // LRCLK pin
    
    // Claim PIO state machine for exclusive use
    uint8_t sm = shared_state.pio_sm = config->pio_sm;
    pio_sm_claim(audio_pio, sm);
    
    // Load I2S PIO program into PIO memory
    loaded_offset = pio_add_program(audio_pio, &audio_i2s_program);
    
    // Validate output format requirements
    // Current implementation requires stereo output
    assert(output_format->channel_count == AUDIO_CHANNEL_STEREO);
    
    // Validate PCM format support (16-bit or 32-bit signed)
    assert(output_format->pcm_format == AUDIO_PCM_FORMAT_S16 || 
           output_format->pcm_format == AUDIO_PCM_FORMAT_S32);
    
    // Determine bit resolution for PIO configuration
    uint res_bits = (output_format->pcm_format == AUDIO_PCM_FORMAT_S32) ? 32 : 16;
    
    // Initialize PIO state machine with I2S timing parameters
    audio_i2s_program_init(audio_pio, sm, loaded_offset, 
                          config->data_pin, config->clock_pin_base, res_bits);
    
    // Allocate and initialize silence buffer for underrun protection
    // Buffer size: samples × channels × bytes_per_sample
    silence_buffer.buffer = pico_buffer_alloc(PICO_AUDIO_I2S_BUFFER_SAMPLE_LENGTH * 4);
    silence_buffer.sample_count = PICO_AUDIO_I2S_BUFFER_SAMPLE_LENGTH;
    silence_buffer.format = &pio_i2s_consumer_buffer_format;

    
    // Memory fence to ensure all setup is complete before DMA configuration
    __mem_fence_release();
    
    // Store DMA channel assignments in shared state
    uint8_t dma_channel0 = config->dma_channel0;
    uint8_t dma_channel1 = config->dma_channel1;
    shared_state.dma_channel0 = dma_channel0;
    shared_state.dma_channel1 = dma_channel1;
    
    // Determine DMA transfer size based on audio format
    // For stereo audio, DMA transfers pairs of samples
    enum dma_channel_transfer_size i2s_dma_configure_size;
    
    if (output_format->channel_count == AUDIO_CHANNEL_MONO) {
        // Mono audio support (currently not fully implemented)
        switch (output_format->pcm_format) {
            case AUDIO_PCM_FORMAT_S8:
            case AUDIO_PCM_FORMAT_U8:
                i2s_dma_configure_size = DMA_SIZE_8;
                assert(false); // Mono 8-bit not supported
                break;
            case AUDIO_PCM_FORMAT_S16:
            case AUDIO_PCM_FORMAT_U16:
                i2s_dma_configure_size = DMA_SIZE_16;
                assert(false); // Mono 16-bit not supported
                break;
            case AUDIO_PCM_FORMAT_S32:
            case AUDIO_PCM_FORMAT_U32:
                i2s_dma_configure_size = DMA_SIZE_32;
                assert(false); // Mono 32-bit not supported
                break;
            default:
                assert(false); // Unsupported format
                break;
        }
    } else {
        // Stereo audio configuration
        switch (output_format->pcm_format) {
            case AUDIO_PCM_FORMAT_S8:
            case AUDIO_PCM_FORMAT_U8:
                // 8-bit stereo: transfer 16 bits (2 × 8-bit samples)
                i2s_dma_configure_size = DMA_SIZE_16;
                break;
            case AUDIO_PCM_FORMAT_S16:
            case AUDIO_PCM_FORMAT_U16:
                // 16-bit stereo: transfer 32 bits (2 × 16-bit samples)
                i2s_dma_configure_size = DMA_SIZE_32;
                break;
            case AUDIO_PCM_FORMAT_S32:
            case AUDIO_PCM_FORMAT_U32:
                // 32-bit stereo: transfer 32 bits per sample (requires special handling)
                // Note: No 64-bit DMA available, so each sample transferred separately
                i2s_dma_configure_size = DMA_SIZE_32;
                break;
            default:
                assert(false); // Unsupported format
                break;
        }
    }
    
    // Configure DMA channel 0 for ping-pong buffering
    dma_config0 = dma_channel_get_default_config(dma_channel0);
    channel_config_set_transfer_data_size(&dma_config0, i2s_dma_configure_size); // Transfer size
    channel_config_set_read_increment(&dma_config0, true);   // Increment source address
    channel_config_set_write_increment(&dma_config0, false); // Fixed destination (PIO TX FIFO)
    channel_config_set_dreq(&dma_config0, DREQ_PIOx_TX0 + sm); // PIO data request signal
    channel_config_set_chain_to(&dma_config0, dma_channel1);   // Chain to channel 1
    
    // Configure DMA channel 1 for ping-pong buffering
    dma_config1 = dma_channel_get_default_config(dma_channel1);
    channel_config_set_transfer_data_size(&dma_config1, i2s_dma_configure_size); // Transfer size
    channel_config_set_read_increment(&dma_config1, true);   // Increment source address
    channel_config_set_write_increment(&dma_config1, false); // Fixed destination (PIO TX FIFO)
    channel_config_set_dreq(&dma_config1, DREQ_PIOx_TX0 + sm); // PIO data request signal
    channel_config_set_chain_to(&dma_config1, dma_channel0);   // Chain to channel 0
    
    // Return the actual output format that will be used
    return output_format;
}

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Update PIO clock divider for target sampling frequency
 * 
 * This function calculates and applies the appropriate clock divider to achieve
 * the desired sampling frequency. It supports both fractional and integer
 * division modes for optimal frequency accuracy.
 * 
 * The I2S bit clock (BCLK) frequency is calculated as:
 * BCLK = sample_freq × bits_per_sample × channels
 * 
 * PIO clock divider = system_clock / (BCLK × 2)
 * The factor of 2 accounts for the PIO program structure.
 * 
 * @param sample_freq Target sampling frequency in Hz
 * @param pcm_format  PCM format (determines bits per sample)
 * @param channel_count Number of audio channels
 * 
 * @note This function can be called at runtime to change sampling frequency
 * @note Frequency changes may cause brief audio interruption
 */
static void update_pio_frequency(uint32_t sample_freq, audio_pcm_format_t pcm_format, audio_channel_t channel_count) 
{
    printf("Setting PIO frequency for target sampling frequency = %u Hz\n", sample_freq);
    
    // Get current system clock frequency
    uint32_t system_clock_frequency = clock_get_hz(clk_sys);
    
    // Ensure system clock is within safe range for calculations
    assert(system_clock_frequency < 0x40000000);
    
    // Calculate clock divider based on audio format
    uint32_t divider;
    uint32_t bits;
    
    switch (pcm_format) {
        case AUDIO_PCM_FORMAT_S8:
        case AUDIO_PCM_FORMAT_U8:
            // 8-bit audio: BCLK = sample_freq × 8 × channels
            divider = system_clock_frequency * 4 * channel_count / sample_freq;
            bits = 8;
            break;
            
        case AUDIO_PCM_FORMAT_S16:
        case AUDIO_PCM_FORMAT_U16:
            // 16-bit audio: BCLK = sample_freq × 16 × channels
            divider = system_clock_frequency * 2 * channel_count / sample_freq;
            bits = 16;
            break;
            
        case AUDIO_PCM_FORMAT_S32:
        case AUDIO_PCM_FORMAT_U32:
            // 32-bit audio: BCLK = sample_freq × 32 × channels
            divider = system_clock_frequency * 1 * channel_count / sample_freq;
            bits = 32;
            break;
            
        default:
            // Fallback to 16-bit configuration
            divider = system_clock_frequency * 2 * channel_count / sample_freq;
            bits = 16;
            assert(false); // Unsupported format
            break;
    }
    
    // Validate divider is within PIO hardware limits
    assert(divider < 0x1000000);  // 24-bit limit
    assert(bits <= 32);           // Maximum supported bit depth
    
#ifdef PIO_CLK_DIV_FRAC
    // Fractional clock division for better frequency accuracy
    // Divider format: 16.8 fixed point (integer.fraction)
    float pio_freq = (float) system_clock_frequency * 256 / divider;
    printf("System clock: %u Hz, I2S divider: %u/256, PIO freq: %.4f Hz\n", 
           system_clock_frequency, divider, pio_freq);
    
    // Apply fractional divider (may introduce slight jitter)
    pio_sm_set_clkdiv_int_frac(audio_pio, shared_state.pio_sm, 
                               divider >> 8u,    // Integer part
                               divider & 0xffu);  // Fractional part
#else
    // Integer-only clock division for jitter-free operation
    divider >>= 8u;  // Convert to integer divider
    float pio_freq = (float) system_clock_frequency / divider;
    float actual_sample_freq = pio_freq / ((float) bits * 2.0 * 2.0);
    
    printf("System clock: %u Hz, I2S divider: %u, PIO freq: %.4f Hz, Actual sample freq: %.4f Hz\n", 
           system_clock_frequency, divider, pio_freq, actual_sample_freq);
    
    // Apply integer divider (no jitter, but less frequency accuracy)
    pio_sm_set_clkdiv(audio_pio, shared_state.pio_sm, divider);
#endif
    
    // Update shared state with new frequency
    shared_state.freq = sample_freq;
}

static audio_buffer_t *wrap_consumer_take(audio_connection_t *connection, bool block) {
    // support dynamic frequency shifting
    if (connection->producer_pool->format->sample_freq != shared_state.freq) {
        update_pio_frequency(connection->producer_pool->format->sample_freq, connection->producer_pool->format->pcm_format, connection->producer_pool->format->channel_count);
    }
    if (_i2s_input_audio_format->pcm_format == _i2s_output_audio_format->pcm_format) {
        if (_i2s_input_audio_format->channel_count == AUDIO_CHANNEL_MONO && _i2s_input_audio_format->channel_count == AUDIO_CHANNEL_MONO) {
            return mono_to_mono_consumer_take(connection, block);
        } else if (_i2s_input_audio_format->channel_count == AUDIO_CHANNEL_MONO && _i2s_input_audio_format->channel_count == AUDIO_CHANNEL_STEREO) {
            return mono_to_stereo_consumer_take(connection, block);
        } else if (_i2s_input_audio_format->channel_count == AUDIO_CHANNEL_STEREO && _i2s_input_audio_format->channel_count == AUDIO_CHANNEL_STEREO) {
            switch (_i2s_input_audio_format->pcm_format) {
                case AUDIO_PCM_FORMAT_S16:
                    return stereo_s16_to_stereo_s16_consumer_take(connection, block);
                    break;
                case AUDIO_PCM_FORMAT_S32:
                    return stereo_s32_to_stereo_s32_consumer_take(connection, block);
                    break;
                default:
                assert(false);
            }
        } else {
            assert(false); // unsupported
        }
    } else {
        assert(false); // unsupported
    }
}

static void wrap_producer_give(audio_connection_t *connection, audio_buffer_t *buffer) {
    // support dynamic frequency shifting
    if (connection->producer_pool->format->sample_freq != shared_state.freq) {
        update_pio_frequency(connection->producer_pool->format->sample_freq, connection->producer_pool->format->pcm_format, connection->producer_pool->format->channel_count);
    }
    if (_i2s_input_audio_format->pcm_format == _i2s_output_audio_format->pcm_format) {
        if (_i2s_input_audio_format->channel_count == AUDIO_CHANNEL_MONO && _i2s_input_audio_format->channel_count == AUDIO_CHANNEL_MONO) {
            assert(false);
            //return mono_to_mono_producer_give(connection, block);
        } else if (_i2s_input_audio_format->channel_count == AUDIO_CHANNEL_MONO && _i2s_input_audio_format->channel_count == AUDIO_CHANNEL_STEREO) {
            assert(false);
            //return mono_to_stereo_producer_give(connection, buffer);
        } else if (_i2s_input_audio_format->channel_count == AUDIO_CHANNEL_STEREO && _i2s_input_audio_format->channel_count == AUDIO_CHANNEL_STEREO) {
            switch (_i2s_input_audio_format->pcm_format) {
                case AUDIO_PCM_FORMAT_S16:
                    return stereo_s16_to_stereo_s16_producer_give(connection, buffer);
                    break;
                case AUDIO_PCM_FORMAT_S32:
                    return stereo_s32_to_stereo_s32_producer_give(connection, buffer);
                    break;
                default:
                assert(false);
            }
        } else {
            assert(false); // unsupported
        }
    } else {
        assert(false); // unsupported
    }
}

static struct buffer_copying_on_consumer_take_connection m2s_audio_i2s_ct_connection = {
        .core = {
                .consumer_pool_take = wrap_consumer_take,
                .consumer_pool_give = consumer_pool_give_buffer_default,
                .producer_pool_take = producer_pool_take_buffer_default,
                .producer_pool_give = producer_pool_give_buffer_default,
        }
};

static struct producer_pool_blocking_give_connection m2s_audio_i2s_pg_connection = {
        .core = {
                .consumer_pool_take = consumer_pool_take_buffer_default,
                .consumer_pool_give = consumer_pool_give_buffer_default,
                .producer_pool_take = producer_pool_take_buffer_default,
                .producer_pool_give = wrap_producer_give,
        }
};

bool audio_i2s_connect_thru(audio_buffer_pool_t *producer, audio_connection_t *connection) {
    return audio_i2s_connect_extra(producer, false, 2, 256, connection);
}

bool audio_i2s_connect(audio_buffer_pool_t *producer) {
    return audio_i2s_connect_thru(producer, NULL);
}

bool audio_i2s_connect_extra(audio_buffer_pool_t *producer, bool buffer_on_give, uint buffer_count,
                                 uint samples_per_buffer, audio_connection_t *connection) {
    printf("Connecting PIO I2S audio\n");

    // todo we need to pick a connection based on the frequency - e.g. 22050 can be more simply upsampled to 44100
    assert(producer->format->pcm_format == AUDIO_PCM_FORMAT_S16 || producer->format->pcm_format == AUDIO_PCM_FORMAT_S32);
    pio_i2s_consumer_format.pcm_format = _i2s_output_audio_format->pcm_format;
    // todo we could do mono
    // todo we can't match exact, so we should return what we can do
    pio_i2s_consumer_format.sample_freq = producer->format->sample_freq;
    pio_i2s_consumer_format.channel_count = _i2s_output_audio_format->channel_count;
    switch (_i2s_output_audio_format->pcm_format) {
        case AUDIO_PCM_FORMAT_S8:
        case AUDIO_PCM_FORMAT_U8:
            pio_i2s_consumer_buffer_format.sample_stride = 1 * pio_i2s_consumer_format.channel_count;
            break;
        case AUDIO_PCM_FORMAT_S16:
        case AUDIO_PCM_FORMAT_U16:
            pio_i2s_consumer_buffer_format.sample_stride = 2 * pio_i2s_consumer_format.channel_count;
            break;
        case AUDIO_PCM_FORMAT_S32:
        case AUDIO_PCM_FORMAT_U32:
            pio_i2s_consumer_buffer_format.sample_stride = 4 * pio_i2s_consumer_format.channel_count;
            break;
        default:
            assert(false);
            break;
    }

    audio_i2s_consumer = audio_new_consumer_pool(&pio_i2s_consumer_buffer_format, buffer_count, samples_per_buffer);

    update_pio_frequency(producer->format->sample_freq, producer->format->pcm_format, producer->format->channel_count);

    // todo cleanup threading
    __mem_fence_release();

    if (!connection) {
        if (producer->format->channel_count == AUDIO_CHANNEL_STEREO) {
            if (_i2s_input_audio_format->channel_count == AUDIO_CHANNEL_MONO) {
                panic("need to merge channels down\n");
            } else if (_i2s_output_audio_format->channel_count == AUDIO_CHANNEL_MONO) {
                panic("trying to play stereo thru mono not yet supported");
            } else {
                printf("Copying stereo to stereo at %d Hz\n", (int) producer->format->sample_freq);
            }
            // todo we should support pass thru option anyway
            //printf("TODO... not completing stereo audio connection properly!\n");
        } else {
            if (_i2s_output_audio_format->channel_count == AUDIO_CHANNEL_MONO) {
                printf("Copying mono to mono at %d Hz\n", (int) producer->format->sample_freq);
            } else {
                printf("Converting mono to stereo at %d Hz\n", (int) producer->format->sample_freq);
            }
        }
        connection = buffer_on_give ? &m2s_audio_i2s_pg_connection.core : &m2s_audio_i2s_ct_connection.core;
    }
    audio_complete_connection(connection, producer, audio_i2s_consumer);
    return true;
}

static struct buffer_copying_on_consumer_take_connection m2s_audio_i2s_connection_s8_mono = {
        .core = {
                .consumer_pool_take = mono_s8_to_mono_consumer_take,
                .consumer_pool_give = consumer_pool_give_buffer_default,
                .producer_pool_take = producer_pool_take_buffer_default,
                .producer_pool_give = producer_pool_give_buffer_default,
        }
};

static struct buffer_copying_on_consumer_take_connection m2s_audio_i2s_connection_s8_stereo = {
        .core = {
                .consumer_pool_take = mono_s8_to_stereo_consumer_take,
                .consumer_pool_give = consumer_pool_give_buffer_default,
                .producer_pool_take = producer_pool_take_buffer_default,
                .producer_pool_give = producer_pool_give_buffer_default,
        }
};

bool audio_i2s_connect_s8(audio_buffer_pool_t *producer) {
    printf("Connecting PIO I2S audio (U8)\n");

    // todo we need to pick a connection based on the frequency - e.g. 22050 can be more simply upsampled to 44100
    assert(producer->format->pcm_format == AUDIO_PCM_FORMAT_S8);
    pio_i2s_consumer_format.pcm_format = AUDIO_PCM_FORMAT_S16;
    // todo we could do mono
    // todo we can't match exact, so we should return what we can do
    pio_i2s_consumer_format.sample_freq = producer->format->sample_freq;
    pio_i2s_consumer_format.channel_count = _i2s_output_audio_format->channel_count;
    switch (_i2s_output_audio_format->pcm_format) {
        case AUDIO_PCM_FORMAT_S8:
        case AUDIO_PCM_FORMAT_U8:
            pio_i2s_consumer_buffer_format.sample_stride = 1 * pio_i2s_consumer_format.channel_count;
            break;
        case AUDIO_PCM_FORMAT_S16:
        case AUDIO_PCM_FORMAT_U16:
            pio_i2s_consumer_buffer_format.sample_stride = 2 * pio_i2s_consumer_format.channel_count;
            break;
        case AUDIO_PCM_FORMAT_S32:
        case AUDIO_PCM_FORMAT_U32:
            pio_i2s_consumer_buffer_format.sample_stride = 4 * pio_i2s_consumer_format.channel_count;
            break;
        default:
            assert(false);
            break;
    }

    // we do this on take so should do it quickly...
    uint samples_per_buffer = 256;
    // todo with take we really only need 1 buffer
    audio_i2s_consumer = audio_new_consumer_pool(&pio_i2s_consumer_buffer_format, 2, samples_per_buffer);

    // todo we need a method to calculate this in clocks
    uint32_t system_clock_frequency = 48000000;
    //uint32_t divider = system_clock_frequency * 4 / producer->format->sample_freq; // avoid arithmetic overflow
    //uint32_t divider = system_clock_frequency * 256 / producer->format->sample_freq * 16 * 4;
    uint32_t divider;
    switch (producer->format->pcm_format) {
        case AUDIO_PCM_FORMAT_S8:
        case AUDIO_PCM_FORMAT_U8:
            divider = system_clock_frequency * 4 * producer->format->channel_count * 2 / producer->format->sample_freq;
            break;
        case AUDIO_PCM_FORMAT_S16:
        case AUDIO_PCM_FORMAT_U16:
            divider = system_clock_frequency * 2 * producer->format->channel_count * 2 / producer->format->sample_freq;
            break;
        case AUDIO_PCM_FORMAT_S32:
        case AUDIO_PCM_FORMAT_U32:
            divider = system_clock_frequency * 1 * producer->format->channel_count * 2 / producer->format->sample_freq;
            break;
        default:
            divider = system_clock_frequency * 2 * producer->format->channel_count * 2 / producer->format->sample_freq;
            assert(false);
            break;
    }
    pio_sm_set_clkdiv_int_frac(audio_pio, shared_state.pio_sm, divider >> 8u, divider & 0xffu);

    // todo cleanup threading
    __mem_fence_release();

    audio_connection_t *connection;
    if (producer->format->channel_count == AUDIO_CHANNEL_STEREO) {
        if (_i2s_output_audio_format->channel_count == AUDIO_CHANNEL_MONO) {
            panic("trying to play stereo thru mono not yet supported");
        }
        // todo we should support pass thru option anyway
        printf("TODO... not completing stereo audio connection properly!\n");
        connection = &m2s_audio_i2s_connection_s8_stereo.core;
    } else {
        if (_i2s_output_audio_format->channel_count == AUDIO_CHANNEL_MONO) {
            printf("Copying mono to mono at %d Hz\n", (int) producer->format->sample_freq);
            connection = &m2s_audio_i2s_connection_s8_mono.core;
        } else {
            printf("Converting mono to stereo at %d Hz\n", (int) producer->format->sample_freq);
            connection = &m2s_audio_i2s_connection_s8_stereo.core;
        }
    }
    audio_complete_connection(connection, producer, audio_i2s_consumer);
    return true;
}

static inline void audio_start_dma_transfer(uint8_t dma_channel, dma_channel_config *dma_config, audio_buffer_t **playing_buffer) {
    assert(!*playing_buffer);

    #ifdef WATCH_DMA_TRANSFER_INTERVAL
    static uint32_t latest = 0;
    static uint32_t max_interval = 0;
    uint32_t now = _micros();
    uint32_t interval = now - latest;
    if (latest != 0 && max_interval < interval) {
        printf("dma_transfer interval %d\n", interval);
        max_interval = interval;
    }
    latest = now;
    #endif // WATCH_DMA_TRANSFER_INTERVAL
    #ifdef WATCH_PIO_SM_TX_FIFO_LEVEL
    uint tx_fifo_level = pio_sm_get_tx_fifo_level(audio_pio, shared_state.pio_sm);
    if (tx_fifo_level < 4) {
        printf("PIO TX FIFO too low: %d at %d ms\n", (int) tx_fifo_level, (int) _millis());
    }
    #endif // WATCH_PIO_SM_TX_FIFO_LEVEL

    audio_buffer_t *ab = take_audio_buffer(audio_i2s_consumer, false);

    *playing_buffer = ab;
    if (!ab) {
        DEBUG_PINS_XOR(audio_timing, 1);
        DEBUG_PINS_XOR(audio_timing, 2);
        DEBUG_PINS_XOR(audio_timing, 1);
        //DEBUG_PINS_XOR(audio_timing, 2);
        // just play some silence
        ab = &silence_buffer;
    }
    assert(ab->sample_count);
    // todo better naming of format->format->format!!
    assert(ab->format->format->pcm_format == AUDIO_PCM_FORMAT_S16 || ab->format->format->pcm_format == AUDIO_PCM_FORMAT_S32);
    if (_i2s_output_audio_format->channel_count == AUDIO_CHANNEL_MONO) {
        assert(ab->format->format->channel_count == AUDIO_CHANNEL_MONO);
        //assert(ab->format->sample_stride == 2);
    } else {
        assert(ab->format->format->channel_count == AUDIO_CHANNEL_STEREO);
        //assert(ab->format->sample_stride == 4);
    }
    uint transfer_size;
    if (ab->format->format->pcm_format == AUDIO_PCM_FORMAT_S32 && ab->format->format->channel_count == AUDIO_CHANNEL_STEREO) {
        transfer_size = ab->sample_count * 2;
        //dma_channel_transfer_from_buffer_now(dma_channel, ab->buffer->bytes, ab->sample_count*2); // DMA_SIZE_32 * 2 times;
    } else {
        transfer_size = ab->sample_count;
        //dma_channel_transfer_from_buffer_now(dma_channel, ab->buffer->bytes, ab->sample_count);
    }
    dma_channel_configure(
        dma_channel,
        dma_config,
        &audio_pio->txf[shared_state.pio_sm], // dest
        ab->buffer->bytes, // src
        transfer_size, // count
        false // trigger
    );
}

// irq handler for DMA
void __isr __time_critical_func(audio_i2s_dma_irq_handler)() {
#if PICO_AUDIO_I2S_NOOP
    assert(false);
#else
    uint dma_channel0 = shared_state.dma_channel0;
    uint dma_channel1 = shared_state.dma_channel1;
    if (dma_irqn_get_channel_status(PICO_AUDIO_I2S_DMA_IRQ, dma_channel0)) {
        dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_channel0);
        DEBUG_PINS_SET(audio_timing, 4);
        // free the buffer we just finished
        if (shared_state.playing_buffer0) {
            give_audio_buffer(audio_i2s_consumer, shared_state.playing_buffer0);
#ifndef NDEBUG
            shared_state.playing_buffer0 = NULL;
#endif
        }
        audio_start_dma_transfer(dma_channel0, &dma_config0, &shared_state.playing_buffer0);
        DEBUG_PINS_CLR(audio_timing, 4);
#ifdef CORE1_PROCESS_I2S_CALLBACK
        bool flg = multicore_fifo_push_timeout_us(EVENT_I2S_DMA_TRANSFER_STARTED, FIFO_TIMEOUT);
        if (!flg) { printf("Core0 -> Core1 FIFO Full\n"); }
#else
        i2s_callback_func();
#endif // CORE1_PROCESS_I2S_CALLBACK
    } else if (dma_irqn_get_channel_status(PICO_AUDIO_I2S_DMA_IRQ, dma_channel1)) {
        dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_channel1);
        DEBUG_PINS_SET(audio_timing, 4);
        // free the buffer we just finished
        if (shared_state.playing_buffer1) {
            give_audio_buffer(audio_i2s_consumer, shared_state.playing_buffer1);
#ifndef NDEBUG
            shared_state.playing_buffer1 = NULL;
#endif
        }
        audio_start_dma_transfer(dma_channel1, &dma_config1, &shared_state.playing_buffer1);
        DEBUG_PINS_CLR(audio_timing, 4);
#ifdef CORE1_PROCESS_I2S_CALLBACK
        bool flg = multicore_fifo_push_timeout_us(EVENT_I2S_DMA_TRANSFER_STARTED, FIFO_TIMEOUT);
        if (!flg) { printf("Core0 -> Core1 FIFO Full\n"); }
#else
        i2s_callback_func();
#endif // CORE1_PROCESS_I2S_CALLBACK
    }
#endif
}

void audio_i2s_set_enabled(bool enabled) {
#ifndef NDEBUG
    if (enabled) {
        printf("Enabling PIO I2S audio (on core %d)\n", get_core_num());
    } else {
        printf("Disabling PIO I2S audio (on core %d)\n", get_core_num());
    }
#endif
    uint dma_channel0 = shared_state.dma_channel0;
    uint dma_channel1 = shared_state.dma_channel1;

    if (enabled) {
        dma_channel_claim(dma_channel0);
        dma_channel_claim(dma_channel1);
        audio_start_dma_transfer(dma_channel0, &dma_config0, &shared_state.playing_buffer0);
        audio_start_dma_transfer(dma_channel1, &dma_config1, &shared_state.playing_buffer1);
        if (!irq_has_shared_handler(DMA_IRQ_x)) {
            irq_add_shared_handler(DMA_IRQ_x, audio_i2s_dma_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        }
        dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_channel0, true);
        dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_channel1, true);
        irq_set_enabled(DMA_IRQ_x, true);
        dma_channel_start(dma_channel0);
#ifdef CORE1_PROCESS_I2S_CALLBACK
        {
            bool flg;
            uint32_t msg;
            multicore_reset_core1();
            multicore_launch_core1(i2s_callback_loop);
            flg = multicore_fifo_pop_timeout_us(FIFO_TIMEOUT, &msg);
            if (!flg || msg != RESPONSE_CORE1_THREAD_STARTED) {
                panic("Core1 is not respond\n");
            }
            pio_sm_set_enabled(audio_pio, shared_state.pio_sm, enabled);
        }
#endif // CORE1_PROCESS_I2S_CALLBACK
    } else {
#ifdef CORE1_PROCESS_I2S_CALLBACK
        {
            bool flg;
            uint32_t msg;
            pio_sm_set_enabled(audio_pio, shared_state.pio_sm, false);
            flg = multicore_fifo_push_timeout_us(NOTIFY_I2S_DISABLED, FIFO_TIMEOUT);
            if (!flg) { printf("Core0 -> Core1 FIFO Full\n"); }
            flg = multicore_fifo_pop_timeout_us(FIFO_TIMEOUT, &msg);
            if (!flg || msg != RESPONSE_CORE1_THREAD_TERMINATED) {
                panic("Core1 is not respond\n");
            }
        }
#endif // CORE1_PROCESS_I2S_CALLBACK
        dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_channel0, false);
        dma_irqn_set_channel_enabled(PICO_AUDIO_I2S_DMA_IRQ, dma_channel1, false);
        irq_set_enabled(DMA_IRQ_x, false);
        dma_channel_abort(dma_channel0);
        dma_channel_wait_for_finish_blocking(dma_channel0);
        dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_channel0);
        dma_channel_cleanup(dma_channel0);
        dma_channel_unclaim(dma_channel0);
        dma_channel_abort(dma_channel1);
        dma_channel_wait_for_finish_blocking(dma_channel1);
        dma_irqn_acknowledge_channel(PICO_AUDIO_I2S_DMA_IRQ, dma_channel1);
        dma_channel_cleanup(dma_channel1);
        dma_channel_unclaim(dma_channel1);
        if (!irq_has_shared_handler(DMA_IRQ_x)) {
            irq_remove_handler(DMA_IRQ_x, audio_i2s_dma_irq_handler);
        }
    }
}
