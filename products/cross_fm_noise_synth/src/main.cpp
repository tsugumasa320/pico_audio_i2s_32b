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

#include <cstdio>
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/audio.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "hardware/spi.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"

// TODO: シンセサイザー機能のヘッダーファイル実装
// #include "synth_config.h"
// #include "fm_engine.h"
// #include "noise_generator.h"
// #include "cross_modulator.h"
// #include "ui_controller.h"
// #include "preset_manager.h"
// #include "analog_multiplexer.h"

// グローバル状態
// static SynthState g_synth_state;
static audio_buffer_pool_t *g_audio_pool;
// static AnalogMultiplexer g_multiplexer;

// グローバル変数
static bool audio_enabled = false;
static uint32_t audio_phase = 0;

/**
 * @brief Core1で実行される音声生成ループ
 */
void core1_audio_loop() {
    printf("Core1 audio processing started\n");
    uint32_t buffer_count = 0;
    
    while (true) {
        audio_buffer_t *buffer = take_audio_buffer(g_audio_pool, true);
        if (!buffer) {
            printf("Failed to get audio buffer!\n");
            continue;
        }

        int32_t *samples = (int32_t *)buffer->buffer->bytes;
        const uint32_t sample_count = buffer->max_sample_count;

        if (audio_enabled) {
            // 440Hz テストトーン生成
            const uint32_t phase_increment = (440UL * 0x10000UL) / 44100UL; // 440Hz @ 44.1kHz
            
            for (uint32_t i = 0; i < sample_count; i++) {
                // 矩形波生成 (最大レベル)
                int32_t sample = (audio_phase & 0x8000) ? 0x7FFFFF00 : -0x7FFFFF00;
                audio_phase += phase_increment;
                
                // ステレオ出力
                samples[i * 2 + 0] = sample;  // Left
                samples[i * 2 + 1] = sample;  // Right
            }
            
            // 最初の数バッファをデバッグ出力
            buffer_count++;
            if (buffer_count <= 5) {
                printf("Buffer %d: sample_count=%d, first_sample=0x%08x\n", 
                       buffer_count, sample_count, samples[0]);
            }
        } else {
            // 無音
            for (uint32_t i = 0; i < sample_count; i++) {
                samples[i * 2 + 0] = 0;  // Left
                samples[i * 2 + 1] = 0;  // Right
            }
        }

        buffer->sample_count = sample_count;
        give_audio_buffer(g_audio_pool, buffer);
        
        // 100バッファごとにデバッグ出力
        if (buffer_count % 100 == 0) {
            printf("Processed %d audio buffers, phase=0x%08x\n", buffer_count, audio_phase);
        }
    }
}

/**
 * @brief システム初期化
 */
bool init_synth() {
    // stdio初期化
    stdio_init_all();
    
    printf("=== Cross FM Noise Synthesizer ===\n");
    printf("Initializing system...\n");
    
    // システムクロック設定 (96MHz動作 - 高精度オーディオのため)
    printf("Setting up system clock to 96MHz...\n");
    
    // USB PLL を 96MHz に設定
    pll_init(pll_usb, 1, 1536 * MHZ, 4, 4);
    
    // USB クロックを 48MHz に設定
    clock_configure(clk_usb,
        0,
        CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
        96 * MHZ,
        48 * MHZ);
        
    // システムクロックを 96MHz に変更
    clock_configure(clk_sys,
        CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
        CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
        96 * MHZ,
        96 * MHZ);
    
    printf("System clock configured to 96MHz\n");
    
    // TODO: ハードウェア制御の実装
    // PWM初期化（LED表示用）
    // gpio_init(PIN_LED_STATUS);
    // gpio_set_dir(PIN_LED_STATUS, GPIO_OUT);
    
    // オーディオシステム初期化
    audio_format_t audio_format = {
        .sample_freq = 44100,
        .pcm_format = AUDIO_PCM_FORMAT_S32,
        .channel_count = AUDIO_CHANNEL_STEREO
    };
    
    audio_buffer_format_t buffer_format = {
        .format = &audio_format,
        .sample_stride = 8  // 32bit stereo (4bytes * 2 channels)
    };
    
    audio_i2s_config_t config = {
        .data_pin = PICO_AUDIO_I2S_DATA_PIN,
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
        .dma_channel0 = 0,
        .dma_channel1 = 1,
        .pio_sm = 0
    };
    
    printf("I2S Config: data_pin=%d, clock_pin_base=%d\n", 
           config.data_pin, config.clock_pin_base);
    printf("Audio format: freq=%d, pcm_format=%d, channels=%d\n",
           audio_format.sample_freq, audio_format.pcm_format, audio_format.channel_count);
    
    const audio_format_t *output_format = audio_i2s_setup(&audio_format, &audio_format, &config);
    if (!output_format) {
        printf("Failed to setup I2S audio\n");
        return false;
    }
    
    printf("I2S setup successful, output format: freq=%d\n", output_format->sample_freq);
    
    // バッファプール作成
    g_audio_pool = audio_new_producer_pool(&buffer_format, 3, 256);
    if (!g_audio_pool) {
        printf("Failed to create audio buffer pool\n");
        return false;
    }
    printf("Audio buffer pool created successfully\n");
    
    // シンセエンジン初期化 (実装必要な場合はコメントを外す)
    // fm_engine_init(&g_synth_state.fm_engine);
    // noise_generator_init(&g_synth_state.noise_gen);
    // cross_modulator_init(&g_synth_state.cross_mod);
    // ui_controller_init(&g_synth_state.ui);
    // preset_manager_init(&g_synth_state.preset_mgr);
    
    // 初期バッファを準備（無音状態）
    {
        audio_buffer_t *ab = take_audio_buffer(g_audio_pool, true);
        int32_t *samples = (int32_t *) ab->buffer->bytes;
        for (uint i = 0; i < ab->max_sample_count; i++) {
            samples[i*2+0] = 0;  // Left
            samples[i*2+1] = 0;  // Right
        }
        ab->sample_count = ab->max_sample_count;
        give_audio_buffer(g_audio_pool, ab);
    }
    
    // オーディオ開始
    printf("Connecting audio pool to I2S...\n");
    bool connect_result = audio_i2s_connect(g_audio_pool);
    if (!connect_result) {
        printf("Failed to connect audio pool to I2S!\n");
        return false;
    }
    printf("Audio pool connected successfully\n");
    
    printf("Enabling I2S output...\n");
    audio_i2s_set_enabled(true);
    printf("I2S output enabled\n");
    
    // Core1で音声処理を開始
    printf("Launching Core1 audio processing...\n");
    multicore_launch_core1(core1_audio_loop);
    
    // 少し待ってから音声出力を有効化
    sleep_ms(500);
    printf("Enabling audio generation...\n");
    audio_enabled = true;
    
    printf("Cross FM Noise Synthesizer initialized\n");
    return true;
}

/**
 * @brief メインループ
 */
int main() {
    if (!init_synth()) {
        printf("Synthesizer initialization failed\n");
        return -1;
    }
    
    printf("Cross FM Noise Synthesizer starting...\n");
    
    // メインループ (Core0はUI制御用)
    while (true) {
        // 現在時刻取得
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        // デバッグ情報を定期的に出力
        static uint32_t last_debug_time = 0;
        if (current_time - last_debug_time > 5000) {  // 5秒ごと
            printf("Cross FM Synth running... Audio Core1 active\n");
            last_debug_time = current_time;
        }
        
        // TODO: UIコントロール処理
        // - ボタン入力
        // - アナログコントロール読み取り
        // - プリセット管理
        
        // 待機
        sleep_ms(100);
    }
    
    return 0;
}