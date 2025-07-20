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

#include "synth_config.h"
#include "simple_fm.h"
#include "simple_noise.h"
#include "cross_mod.h"

// グローバル状態
static audio_buffer_pool_t *g_audio_pool;
static SimpleFM g_fm_synth;
static SimpleNoise g_noise_gen;
static CrossMod g_cross_mod;

// パラメーター制御
static struct {
    float fm_frequency = 440.0f;
    float fm_ratio = 1.5f;
    float fm_index = 2.0f;
    float noise_level = 0.3f;
    SimpleNoise::NoiseType noise_type = SimpleNoise::WHITE_NOISE;
    float cross_depth = 0.5f;
    float cross_rate = 0.8f;
    float master_volume = 0.7f;
} g_params;

// グローバル変数
static bool audio_enabled = false;
static uint32_t audio_phase = 0;
static constexpr int32_t DAC_ZERO = 1;  // DACのゼロレベル

/**
 * @brief Core1で実行されるFMシンセサイザー音声生成ループ
 */
void core1_audio_loop() {
    printf("Core1 FM Synthesizer processing started\n");
    uint32_t buffer_count = 0;
    
    // シンセサイザー初期化
    g_fm_synth.Init(44100.0f);
    g_noise_gen.Init();
    g_cross_mod.Init(44100.0f);
    
    // 初期パラメーター設定
    g_fm_synth.SetFrequency(g_params.fm_frequency);
    g_fm_synth.SetRatio(g_params.fm_ratio);
    g_fm_synth.SetIndex(g_params.fm_index);
    g_noise_gen.SetLevel(g_params.noise_level);
    g_noise_gen.SetType(g_params.noise_type);
    g_cross_mod.SetDepth(g_params.cross_depth);
    g_cross_mod.SetRate(g_params.cross_rate);
    
    printf("FM Synthesizer initialized\n");
    
    while (true) {
        audio_buffer_t *buffer = take_audio_buffer(g_audio_pool, true);
        if (!buffer) {
            printf("Failed to get audio buffer!\n");
            continue;
        }

        int32_t *samples = (int32_t *)buffer->buffer->bytes;
        const uint32_t sample_count = buffer->max_sample_count;

        if (audio_enabled) {
            for (uint32_t i = 0; i < sample_count; i++) {
                // FM合成処理
                float fm_output = g_fm_synth.Process();
                
                // ノイズ生成
                float noise_output = g_noise_gen.Process();
                
                // クロスモジュレーション処理
                float cross_output = g_cross_mod.Process(fm_output, noise_output);
                
                // 最終ミックス
                float final_output = (fm_output * 0.4f + 
                                    noise_output * 0.3f + 
                                    cross_output * 0.3f) * g_params.master_volume;
                
                // クリッピング防止
                if (final_output > 1.0f) final_output = 1.0f;
                if (final_output < -1.0f) final_output = -1.0f;
                
                // 32bit signed integerに変換
                int32_t sample = (int32_t)(final_output * 0x7FFFFF00);
                
                // ステレオ出力
                samples[i * 2 + 0] = sample;  // Left
                samples[i * 2 + 1] = sample;  // Right
            }
            
            // 最初の数バッファをデバッグ出力
            buffer_count++;
            if (buffer_count <= 3) {
                printf("FM Buffer %d: sample_count=%d, first_sample=0x%08x\n", 
                       buffer_count, sample_count, samples[0]);
            }
        } else {
            // 無音
            for (uint32_t i = 0; i < sample_count; i++) {
                samples[i * 2 + 0] = DAC_ZERO;  // Left
                samples[i * 2 + 1] = DAC_ZERO;  // Right
            }
        }

        buffer->sample_count = sample_count;
        give_audio_buffer(g_audio_pool, buffer);
        
        // 500バッファごとにデバッグ出力
        if (buffer_count % 500 == 0) {
            printf("FM Synth: %d buffers processed\n", buffer_count);
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
    
    // USBシリアル安定化のための待機
    sleep_ms(2000);
    
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
        
    // 周辺機器クロックもシステムクロックに合わせて変更
    clock_configure(clk_peri,
        0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
        96 * MHZ,
        96 * MHZ);
    
    // クロック変更後にUARTを再初期化
    stdio_init_all();
    
    printf("System clock configured to 96MHz\n");
    
    // DCDC電源制御（オーディオノイズ低減）
    const uint32_t PIN_DCDC_PSM_CTRL = 23;
    gpio_init(PIN_DCDC_PSM_CTRL);
    gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
    gpio_put(PIN_DCDC_PSM_CTRL, 1); // PWMモードでオーディオノイズを低減
    printf("DCDC configured for low-noise audio\n");
    
    // TODO: その他のハードウェア制御の実装
    // PWM初期化（LED表示用）
    // gpio_init(PIN_LED_STATUS);
    // gpio_set_dir(PIN_LED_STATUS, GPIO_OUT);
    
    // オーディオシステム初期化 (sine_wave_i2s_32bと同じ設定)
    
    // sine_wave_i2s_32bと同じバッファサイズを使用
    #define SAMPLES_PER_BUFFER 1156
    
    static audio_format_t audio_format = {
        .sample_freq = 44100,                // サンプリング周波数: 44.1kHz (CD品質)
        .pcm_format = AUDIO_PCM_FORMAT_S32,  // PCMフォーマット: 32bit signed
        .channel_count = AUDIO_CHANNEL_STEREO // チャンネル数: ステレオ (2ch)
    };
    
    static audio_buffer_format_t producer_format = {
        .format = &audio_format,
        .sample_stride = 8                   // ステレオ32bitなので8バイト/サンプル
    };
    
    static audio_i2s_config_t i2s_config = {
        .data_pin = PICO_AUDIO_I2S_DATA_PIN,         // データピン (デフォルト: GP18)
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE, // クロックピンベース (デフォルト: GP16)
        .dma_channel0 = 0,                           // DMAチャンネル0
        .dma_channel1 = 1,                           // DMAチャンネル1
        .pio_sm = 0                                  // PIOステートマシン番号
    };
    
    printf("I2S Config: data_pin=%d, clock_pin_base=%d\n", 
           i2s_config.data_pin, i2s_config.clock_pin_base);
    printf("Audio format: freq=%d, pcm_format=%d, channels=%d\n",
           audio_format.sample_freq, audio_format.pcm_format, audio_format.channel_count);
    
    // オーディオバッファプールを作成 (sine_wave_i2s_32bと同じ)
    g_audio_pool = audio_new_producer_pool(&producer_format, 3, SAMPLES_PER_BUFFER);
    if (!g_audio_pool) {
        printf("Failed to create audio buffer pool\n");
        return false;
    }
    printf("Audio buffer pool created successfully\n");
    
    // I2Sハードウェアをセットアップ (sine_wave_i2s_32bと同じ)
    const audio_format_t *output_format = audio_i2s_setup(&audio_format, &audio_format, &i2s_config);
    if (!output_format) {
        printf("PicoAudio: Unable to open audio device.\n");
        return false;
    }
    
    printf("I2S setup successful, output format: freq=%d\n", output_format->sample_freq);
    
    // シンセエンジン初期化 (実装必要な場合はコメントを外す)
    // fm_engine_init(&g_synth_state.fm_engine);
    // noise_generator_init(&g_synth_state.noise_gen);
    // cross_modulator_init(&g_synth_state.cross_mod);
    // ui_controller_init(&g_synth_state.ui);
    // preset_manager_init(&g_synth_state.preset_mgr);
    
    // バッファプールをI2S出力に接続 (sine_wave_i2s_32bと同じ)
    printf("Connecting audio pool to I2S...\n");
    bool connect_result = audio_i2s_connect(g_audio_pool);
    if (!connect_result) {
        printf("Failed to connect audio pool to I2S!\n");
        return false;
    }
    printf("Audio pool connected successfully\n");
    
    // 初期バッファデータを設定（無音状態）- sine_wave_i2s_32bと同じ
    {
        audio_buffer_t *ab = take_audio_buffer(g_audio_pool, true);
        int32_t *samples = (int32_t *) ab->buffer->bytes;
        for (uint i = 0; i < ab->max_sample_count; i++) {
            samples[i*2+0] = DAC_ZERO;  // 左チャンネル
            samples[i*2+1] = DAC_ZERO;  // 右チャンネル
        }
        ab->sample_count = ab->max_sample_count;
        give_audio_buffer(g_audio_pool, ab);
    }
    
    // I2S出力を有効化 (sine_wave_i2s_32bと同じ)
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
    
    // メインループ (Core0はUI制御とパラメーター変更用)
    while (true) {
        // 現在時刻取得
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        // パラメーター動的変更（デモ用）
        static uint32_t last_param_time = 0;
        if (current_time - last_param_time > 2000) {  // 2秒ごとにパラメーター変更
            // FM周波数をゆっくり変化
            static float freq_direction = 1.0f;
            g_params.fm_frequency += freq_direction * 20.0f;
            if (g_params.fm_frequency > 800.0f) {
                g_params.fm_frequency = 800.0f;
                freq_direction = -1.0f;
            } else if (g_params.fm_frequency < 200.0f) {
                g_params.fm_frequency = 200.0f;
                freq_direction = 1.0f;
            }
            
            // FM比率も変化
            static float ratio_direction = 1.0f;
            g_params.fm_ratio += ratio_direction * 0.1f;
            if (g_params.fm_ratio > 3.0f) {
                g_params.fm_ratio = 3.0f;
                ratio_direction = -1.0f;
            } else if (g_params.fm_ratio < 0.5f) {
                g_params.fm_ratio = 0.5f;
                ratio_direction = 1.0f;
            }
            
            // シンセサイザーパラメーター更新
            g_fm_synth.SetFrequency(g_params.fm_frequency);
            g_fm_synth.SetRatio(g_params.fm_ratio);
            
            last_param_time = current_time;
        }
        
        // デバッグ情報を定期的に出力
        static uint32_t last_debug_time = 0;
        if (current_time - last_debug_time > 8000) {  // 8秒ごと
            printf("FM Synth: freq=%.1fHz, ratio=%.2f, index=%.2f\n", 
                   g_params.fm_frequency, g_params.fm_ratio, g_params.fm_index);
            last_debug_time = current_time;
        }
        
        // TODO: 本格的なUIコントロール処理
        // - エンコーダー入力
        // - ボタン入力
        // - アナログマルチプレクサー読み取り
        // - プリセット管理
        
        // 待機
        sleep_ms(100);
    }
    
    return 0;
}