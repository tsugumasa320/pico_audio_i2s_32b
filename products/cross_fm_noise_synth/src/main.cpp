/**
 * @file main.cpp
 * @brief Cross FM Noise Synthesizer - メインプログラム
 * 
 * クロスFMとノイズを組み合わせた実験的なシンセサイザー
 * - 4オペレータークロスFM
 * - マルチノイズソース
 * - リアルタイムパラメーター制御
 * - プリセット管理
 */

#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/audio.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"

#include "synth_config.h"
#include "fm_engine.h"
#include "noise_generator.h"
#include "cross_modulator.h"
#include "ui_controller.h"
#include "preset_manager.h"
#include "analog_multiplexer.h"

// グローバル状態
static SynthState g_synth_state;
static audio_buffer_pool_t *g_audio_pool;
static AnalogMultiplexer g_multiplexer;

/**
 * @brief オーディオコールバック - リアルタイム音声生成
 */
void audio_callback() {
    audio_buffer_t *buffer = take_audio_buffer(g_audio_pool, false);
    if (!buffer) return;

    int32_t *samples = (int32_t *)buffer->buffer->bytes;
    const uint32_t sample_count = buffer->max_sample_count;

    // FM + ノイズ + クロスモジュレーション処理
    for (uint32_t i = 0; i < sample_count; i++) {
        // FMエンジンからの出力
        int32_t fm_output = fm_engine_process(&g_synth_state.fm_engine);
        
        // ノイズジェネレーターからの出力
        int32_t noise_output = noise_generator_process(&g_synth_state.noise_gen);
        
        // クロスモジュレーション処理
        int32_t cross_mod = cross_modulator_process(&g_synth_state.cross_mod, 
                                                    fm_output, noise_output);
        
        // 最終ミックス
        int32_t final_output = (fm_output + noise_output + cross_mod) / 3;
        
        // ステレオ出力
        samples[i * 2 + 0] = final_output;  // Left
        samples[i * 2 + 1] = final_output;  // Right
    }

    buffer->sample_count = sample_count;
    give_audio_buffer(g_audio_pool, buffer);
}

/**
 * @brief システム初期化
 */
bool init_synth() {
    // stdio初期化
    stdio_init_all();
    
    // アナログマルチプレクサ初期化
    MultiplexerConfig mux_config = {
        .pin_enable = PIN_ENCODER_A,      // GP2をマルチプレクサ制御に転用
        .pin_s0 = PIN_ENCODER_B,          // GP3
        .pin_s1 = PIN_ENCODER_SW,         // GP4
        .pin_s2 = PIN_BUTTON_PRESET,      // GP5
        .adc_channel = 0,                 // ADC0 (GP26)
        .scan_period_ms = MUX_DEFAULT_SCAN_PERIOD_MS,
        .is_enable_active_low = true
    };
    multiplexer_init(&g_multiplexer, &mux_config);
    
    // PWM初期化（LED表示用）
    gpio_init(PIN_LED_STATUS);
    gpio_set_dir(PIN_LED_STATUS, GPIO_OUT);
    
    // オーディオシステム初期化
    audio_format_t audio_format = {
        .sample_freq = 44100,
        .format = AUDIO_BUFFER_FORMAT_PCM_S32,
        .channel_count = 2
    };
    
    struct audio_i2s_config config = audio_i2s_default_config();
    audio_i2s_setup(&audio_format, &config);
    
    // バッファプール作成
    g_audio_pool = audio_new_producer_pool(&audio_format, 3, 256);
    
    // シンセエンジン初期化
    fm_engine_init(&g_synth_state.fm_engine);
    noise_generator_init(&g_synth_state.noise_gen);
    cross_modulator_init(&g_synth_state.cross_mod);
    ui_controller_init(&g_synth_state.ui);
    preset_manager_init(&g_synth_state.preset_mgr);
    
    // オーディオ開始
    audio_i2s_connect(g_audio_pool);
    audio_i2s_set_enabled(true);
    
    printf("Cross FM Noise Synthesizer initialized\\n");
    return true;
}

/**
 * @brief メインループ
 */
int main() {
    if (!init_synth()) {
        printf("Synthesizer initialization failed\\n");
        return -1;
    }
    
    printf("Cross FM Noise Synthesizer starting...\\n");
    
    // メインループ
    while (true) {
        // アナログマルチプレクサ更新
        multiplexer_update(&g_multiplexer);
        
        // アナログコントロールからパラメーター更新
        static uint32_t last_param_update = 0;
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        if (current_time - last_param_update > 10) {  // 100Hz更新
            // FM パラメーター制御
            float fm_freq = multiplexer_get_float_value(&g_multiplexer, 0) * 1000.0f + 100.0f;  // 100Hz-1100Hz
            float fm_ratio = multiplexer_get_float_value(&g_multiplexer, 1) * 10.0f + 0.5f;     // 0.5-10.5
            float fm_index = multiplexer_get_float_value(&g_multiplexer, 2) * 20.0f;             // 0-20
            
            // ノイズパラメーター制御
            float noise_level = multiplexer_get_float_value(&g_multiplexer, 3);                  // 0.0-1.0
            uint8_t noise_type = (uint8_t)(multiplexer_get_float_value(&g_multiplexer, 4) * NOISE_COUNT);
            
            // クロスモジュレーション制御
            float cross_depth = multiplexer_get_float_value(&g_multiplexer, 5);                  // 0.0-1.0
            float cross_rate = multiplexer_get_float_value(&g_multiplexer, 6) * 10.0f + 0.1f;   // 0.1-10.1Hz
            
            // マスターボリューム
            float master_vol = multiplexer_get_float_value(&g_multiplexer, 7);                   // 0.0-1.0
            
            // パラメーター適用
            g_synth_state.fm_engine.base_frequency = fm_freq;
            g_synth_state.fm_engine.operators[0].ratio = fm_ratio;
            g_synth_state.noise_gen.level = noise_level * master_vol;
            g_synth_state.noise_gen.type = (NoiseType)(noise_type % NOISE_COUNT);
            g_synth_state.cross_mod.depth = cross_depth;
            g_synth_state.cross_mod.rate = cross_rate;
            
            last_param_update = current_time;
        }
        
        // UI処理（ボタンの読み取り）
        ui_controller_update(&g_synth_state.ui, &g_synth_state);
        
        // プリセット管理
        preset_manager_update(&g_synth_state.preset_mgr, &g_synth_state);
        
        // オーディオコールバックは別スレッドで実行
        audio_callback();
        
        // LEDステータス更新
        static uint32_t led_toggle_time = 0;
        if (current_time - led_toggle_time > 500) {  // 0.5秒間隔で点滅
            gpio_xor_mask(1u << PIN_LED_STATUS);
            led_toggle_time = current_time;
        }
        
        // 少し待機（CPU負荷軽減）
        sleep_ms(1);
    }
    
    return 0;
}