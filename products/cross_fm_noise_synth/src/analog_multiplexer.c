/**
 * @file analog_multiplexer.c
 * @brief Analog Multiplexer 74HC4051 implementation for Raspberry Pi Pico
 */

#include "analog_multiplexer.h"
#include "pico/time.h"
#include "hardware/gpio.h"

static void select_input(const MultiplexerConfig *config, uint input) {
    if (input >= MUX_NUM_INPUTS) return;
    
    gpio_put(config->pin_s0, (input >> 0) & 0x01);
    gpio_put(config->pin_s1, (input >> 1) & 0x01);
    gpio_put(config->pin_s2, (input >> 2) & 0x01);
}

static uint16_t get_averaged_reading(const MultiplexerConfig *config) {
    uint32_t sum = 0;
    for (int i = 0; i < MUX_NUM_AVERAGE; i++) {
        sum += adc_read();
        sleep_us(10);  // Small delay between readings
    }
    return (uint16_t)(sum / MUX_NUM_AVERAGE);
}

void multiplexer_init(AnalogMultiplexer *mux, const MultiplexerConfig *config) {
    if (!mux || !config) return;
    
    mux->config = *config;
    mux->scan_index = 0;
    mux->last_scan_time = 0;
    mux->initialized = false;
    
    // Configure GPIO pins
    gpio_init(config->pin_enable);
    gpio_init(config->pin_s0);
    gpio_init(config->pin_s1);
    gpio_init(config->pin_s2);
    
    gpio_set_dir(config->pin_enable, GPIO_OUT);
    gpio_set_dir(config->pin_s0, GPIO_OUT);
    gpio_set_dir(config->pin_s1, GPIO_OUT);
    gpio_set_dir(config->pin_s2, GPIO_OUT);
    
    // Initialize ADC
    adc_init();
    adc_gpio_init(26 + config->adc_channel);  // GP26, GP27, or GP28
    adc_select_input(config->adc_channel);
    
    // Initialize filters
    for (int i = 0; i < MUX_NUM_INPUTS; i++) {
        mux->filters[i].alpha = 0.8f;  // Smoothing factor
        mux->filters[i].output = 0.0f;
        mux->raw_values[i] = 0;
    }
    
    // Enable multiplexer
    gpio_put(config->pin_enable, config->is_enable_active_low ? 0 : 1);
    sleep_ms(1);
    
    // Initial scan
    for (int i = 0; i < MUX_NUM_INPUTS; i++) {
        select_input(config, i);
        sleep_ms(1);
        uint16_t value = get_averaged_reading(config);
        mux->raw_values[i] = value;
        mux->filters[i].output = (float)value;
    }
    
    mux->last_scan_time = to_ms_since_boot(get_absolute_time());
    mux->initialized = true;
}

void multiplexer_update(AnalogMultiplexer *mux) {
    if (!mux || !mux->initialized) return;
    
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    uint32_t delta_time = current_time - mux->last_scan_time;
    
    if (delta_time < mux->config.scan_period_ms) {
        return;
    }
    
    mux->last_scan_time = current_time;
    
    // Read current input
    uint16_t raw_value = get_averaged_reading(&mux->config);
    mux->raw_values[mux->scan_index] = raw_value;
    
    // Update filter
    OnePoleFilter *filter = &mux->filters[mux->scan_index];
    float input = (float)raw_value;
    filter->output = filter->alpha * input + (1.0f - filter->alpha) * filter->output;
    
    // Prepare for next input
    mux->scan_index = (mux->scan_index + 1) % MUX_NUM_INPUTS;
    select_input(&mux->config, mux->scan_index);
    gpio_put(mux->config.pin_enable, mux->config.is_enable_active_low ? 0 : 1);
}

uint16_t multiplexer_get_raw_value(const AnalogMultiplexer *mux, uint input) {
    if (!mux || input >= MUX_NUM_INPUTS) return 0;
    return mux->raw_values[input];
}

uint16_t multiplexer_get_filtered_value(const AnalogMultiplexer *mux, uint input) {
    if (!mux || input >= MUX_NUM_INPUTS) return 0;
    return (uint16_t)(mux->filters[input].output);
}

float multiplexer_get_float_value(const AnalogMultiplexer *mux, uint input) {
    if (!mux || input >= MUX_NUM_INPUTS) return 0.0f;
    return mux->filters[input].output / 4095.0f;  // Normalize to 0.0-1.0
}