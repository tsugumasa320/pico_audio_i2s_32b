/**
 * @file analog_multiplexer.h
 * @brief Analog Multiplexer 74HC4051 for Raspberry Pi Pico
 */

#ifndef ANALOG_MULTIPLEXER_H
#define ANALOG_MULTIPLEXER_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MUX_NUM_INPUTS 8
#define MUX_DEFAULT_SCAN_PERIOD_MS 1
#define MUX_NUM_AVERAGE 4

/**
 * @brief Analog multiplexer configuration
 */
typedef struct {
    uint pin_enable;    // Enable pin (active low)
    uint pin_s0;        // Select pin S0
    uint pin_s1;        // Select pin S1 
    uint pin_s2;        // Select pin S2
    uint adc_channel;   // ADC channel (0-2 for GP26-28)
    uint scan_period_ms;
    bool is_enable_active_low;
} MultiplexerConfig;

/**
 * @brief One pole IIR filter for smoothing
 */
typedef struct {
    float alpha;
    float output;
} OnePoleFilter;

/**
 * @brief Multiplexer state
 */
typedef struct {
    MultiplexerConfig config;
    OnePoleFilter filters[MUX_NUM_INPUTS];
    uint32_t last_scan_time;
    uint scan_index;
    uint16_t raw_values[MUX_NUM_INPUTS];
    bool initialized;
} AnalogMultiplexer;

/**
 * @brief Initialize multiplexer
 */
void multiplexer_init(AnalogMultiplexer *mux, const MultiplexerConfig *config);

/**
 * @brief Update multiplexer readings
 */
void multiplexer_update(AnalogMultiplexer *mux);

/**
 * @brief Get raw ADC value for input
 */
uint16_t multiplexer_get_raw_value(const AnalogMultiplexer *mux, uint input);

/**
 * @brief Get filtered value for input (0-4095)
 */
uint16_t multiplexer_get_filtered_value(const AnalogMultiplexer *mux, uint input);

/**
 * @brief Get normalized float value (0.0-1.0)
 */
float multiplexer_get_float_value(const AnalogMultiplexer *mux, uint input);

#ifdef __cplusplus
}
#endif

#endif // ANALOG_MULTIPLEXER_H