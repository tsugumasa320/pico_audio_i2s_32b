#ifndef ANALOG_MUX_H
#define ANALOG_MUX_H

#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

/**
 * @brief 74HC4051アナログマルチプレクサー制御クラス
 * 
 * 8チャンネルのアナログ入力を1つのADCで読み取り
 */
class AnalogMux
{
public:
    static constexpr int NUM_CHANNELS = 8;
    static constexpr int DEFAULT_SCAN_PERIOD_MS = 10;

    struct Config {
        uint pin_enable;     // Enable pin (active low)
        uint pin_s0;         // Select pin S0
        uint pin_s1;         // Select pin S1  
        uint pin_s2;         // Select pin S2
        uint adc_pin;        // ADC input pin (26, 27, 28)
        uint adc_channel;    // ADC channel (0, 1, 2)
        uint scan_period_ms; // Scan period in milliseconds
        bool enable_active_low; // Enable pin polarity
    };

    // コンストラクタ
    AnalogMux() : last_scan_time_(0), current_channel_(0), config_({}) {
        // 配列を初期化
        for (int i = 0; i < NUM_CHANNELS; i++) {
            raw_values_[i] = 0;
            float_values_[i] = 0.0f;
        }
        // config_は Init() で適切な値に設定される
    }

    void Init(const Config& config)
    {
        config_ = config;
        
        // GPIO初期化
        gpio_init(config_.pin_enable);
        gpio_init(config_.pin_s0);
        gpio_init(config_.pin_s1);
        gpio_init(config_.pin_s2);
        
        gpio_set_dir(config_.pin_enable, GPIO_OUT);
        gpio_set_dir(config_.pin_s0, GPIO_OUT);
        gpio_set_dir(config_.pin_s1, GPIO_OUT);
        gpio_set_dir(config_.pin_s2, GPIO_OUT);
        
        // ADC初期化
        adc_init();
        adc_gpio_init(config_.adc_pin);
        
        // 初期状態：無効化
        SetEnable(false);
        
        // バッファ初期化
        for (int i = 0; i < NUM_CHANNELS; i++) {
            raw_values_[i] = 0;
            float_values_[i] = 0.0f;
        }
        
        last_scan_time_ = 0;
        current_channel_ = 0;
    }

    void Update()
    {
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        if (current_time - last_scan_time_ >= config_.scan_period_ms) {
            ScanCurrentChannel();
            
            // 次のチャンネルに移動
            current_channel_ = (current_channel_ + 1) % NUM_CHANNELS;
            last_scan_time_ = current_time;
        }
    }

    uint16_t GetRawValue(int channel) const
    {
        if (channel >= 0 && channel < NUM_CHANNELS) {
            return raw_values_[channel];
        }
        return 0;
    }

    float GetFloatValue(int channel) const
    {
        if (channel >= 0 && channel < NUM_CHANNELS) {
            return float_values_[channel];
        }
        return 0.0f;
    }

    // 0.0-1.0の範囲で値を取得
    float GetNormalizedValue(int channel) const
    {
        return GetFloatValue(channel);
    }

    // 指定範囲にマッピングされた値を取得
    float GetMappedValue(int channel, float min_val, float max_val) const
    {
        float normalized = GetNormalizedValue(channel);
        return min_val + normalized * (max_val - min_val);
    }

private:
    Config config_;
    uint16_t raw_values_[NUM_CHANNELS];
    float float_values_[NUM_CHANNELS];
    uint32_t last_scan_time_;
    int current_channel_;

    void SetEnable(bool enable)
    {
        bool output_level = config_.enable_active_low ? !enable : enable;
        gpio_put(config_.pin_enable, output_level);
    }

    void SelectChannel(int channel)
    {
        gpio_put(config_.pin_s0, (channel & 0x01) ? 1 : 0);
        gpio_put(config_.pin_s1, (channel & 0x02) ? 1 : 0);
        gpio_put(config_.pin_s2, (channel & 0x04) ? 1 : 0);
    }

    void ScanCurrentChannel()
    {
        // チャンネル選択
        SelectChannel(current_channel_);
        
        // マルチプレクサー有効化
        SetEnable(true);
        
        // 少し待つ（切り替え時間）
        sleep_us(10);
        
        // ADC読み取り
        adc_select_input(config_.adc_channel);
        uint16_t raw_value = adc_read();
        
        // マルチプレクサー無効化
        SetEnable(false);
        
        // 値を保存
        raw_values_[current_channel_] = raw_value;
        float_values_[current_channel_] = (float)raw_value / 4095.0f; // 12bit ADC
    }
};

#endif // ANALOG_MUX_H