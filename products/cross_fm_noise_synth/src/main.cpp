/**
 * @file main.cpp
 * @brief Cross FM Noise Synthesizer - 参照版（pico2_i2s_pio）の完全再現
 * 
 * 2つのFMシンセが相互に変調し合う実験的なシンセサイザー
 * - DaisySP Fm2クラスを使用
 * - アナログマルチプレクサーによる8ノブ制御
 * - オーバードライブ + アンチエイリアスフィルター + DCブロック
 * - リアルタイムクロスモジュレーション
 */

#include <cstdio>
#include <cmath>
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/audio.h"
#include "pico/multicore.h"
#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/gpio.h"

// DaisySP includes
#include "daisysp.h"

#include "../include/analog_mux.h"
#include "../include/biquad_rbj.h"

using namespace daisysp;

// グローバル状態
static audio_buffer_pool_t *g_audio_pool;

// DaisySP オーディオ処理オブジェクト
static Fm2 fm1, fm2;            // 2つのFMシンセ
static Overdrive overdrive;     // オーバードライブエフェクト
static DcBlock dcBlock;         // 直流オフセット除去フィルタ
static BiquadRBJ antiAliasFilter1, antiAliasFilter2; // アンチエイリアスフィルター

// アナログマルチプレクサー
static AnalogMux g_analog_mux;

// 参照版と同じピン設定
enum {
    kPinNEnable = 0,  // Enable pin (active low)
    kPinS0      = 3,  // Select pin S0
    kPinS1      = 2,  // Select pin S1
    kPinS2      = 1,  // Select pin S2
    kAnalogIn   = 26, // ADC input pin
};

// 設定可能なバッファサイズ
#ifndef SAMPLES_PER_BUFFER
#define SAMPLES_PER_BUFFER 64   // 低レイテンシー（参照版に近い）
// #define SAMPLES_PER_BUFFER 128  // バランス型
// #define SAMPLES_PER_BUFFER 256  // 標準
// #define SAMPLES_PER_BUFFER 1156 // 大きなバッファ（元の値）
#endif

// デバッグ用フラグ
#define DEBUG_FALLBACK_SINE 0   // 1にすると問題切り分け用のサイン波に切り替え（テスト中）

// グローバル変数
static bool audio_enabled = false;
static constexpr int32_t DAC_ZERO = 1;  // DACのゼロレベル

// 参照版のscaleValue関数
float scaleValue(int input, int input_min, int input_max, float output_min, float output_max, float curve = 1.0f)
{
    // 入力値を0～1の範囲に正規化
    float normalized = float(input - input_min) / float(input_max - input_min);
    
    // カーブを適用（curve = 1.0f なら線形、curve > 1.0f で指数カーブ、curve < 1.0f で対数カーブ）
    if (curve != 1.0f) {
        normalized = pow(normalized, curve);
    }
    
    // 出力範囲にスケーリング
    return output_min + normalized * (output_max - output_min);
}

// 参照版のdbtoa関数
inline float dbtoa(float dB)
{
    // 10^(dB / 20) を計算
    return expf(0.11512925464970229f * dB); // 0.11512925464970229 = ln(10) / 20
}

// DaisySPのfclampを使用

/**
 * @brief Core1で実行されるオーディオ処理ループ（参照版の完全再現）
 */
void core1_audio_loop() {
    printf("Core1 FM Cross-Modulation processing started\n");
    uint32_t buffer_count = 0;
    
    // **LEDデバッグ：音声処理の状態を視覚化**
    const uint LED_PIN = 25;  // Pico 2の内蔵LED
    static bool led_state = false;
    
    // **参照版の2つのFMシンセ初期化**
    const float sample_rate = 48000.0f;
    
    printf("Initializing DaisySP Cross FM synth at %.0fHz...\n", sample_rate);
    
    // FM1初期化（参照版と同じ設定）
    fm1.Init(sample_rate);
    fm1.SetFrequency(440.0f);
    fm1.SetRatio(0.5f);
    fm1.SetIndex(100.0f);
    printf("FM1 initialized: 440Hz, ratio=0.5, index=100\n");
    
    // FM2初期化（参照版と同じ設定）
    fm2.Init(sample_rate);
    fm2.SetFrequency(330.0f);
    fm2.SetRatio(0.33f);
    fm2.SetIndex(50.0f);
    printf("FM2 initialized: 330Hz, ratio=0.33, index=50\n");
    
    // オーバードライブ初期化（参照版と同じ）
    overdrive.Init();
    overdrive.SetDrive(0.5f);
    printf("Overdrive initialized with drive=0.5\n");
    
    printf("Cross FM synthesizer with overdrive initialized successfully\n");
    
    // 参照版と完全同じ変数
    static float out1, out2, mixed_out;
    static int32_t sample;
    static float volume = 0.8f; // 参照版と同じデフォルトボリューム
    
    while (true) {
        audio_buffer_t *buffer = take_audio_buffer(g_audio_pool, true);
        if (!buffer) {
            printf("Failed to get audio buffer!\n");
            continue;
        }

        int32_t *samples = (int32_t *)buffer->buffer->bytes;
        const uint32_t sample_count = buffer->max_sample_count;

        if (audio_enabled) {
            // **LEDデバッグ：音声処理中であることを示す（1秒ごとに点滅）**
            if (buffer_count % 750 == 0) {  // 約1秒ごと（64samples×750≈48000samples≈1秒）
                led_state = !led_state;
                gpio_put(LED_PIN, led_state);
            }
            
            // アナログマルチプレクサーの値を取得（参照版と完全同じ）
            g_analog_mux.Update();
            const int val0 = (int)(g_analog_mux.GetNormalizedValue(0) * 1023);
            const int val1 = (int)(g_analog_mux.GetNormalizedValue(1) * 1023);
            const int val2 = (int)(g_analog_mux.GetNormalizedValue(2) * 1023);
            const int val3 = (int)(g_analog_mux.GetNormalizedValue(3) * 1023);
            const int val4 = (int)(g_analog_mux.GetNormalizedValue(4) * 1023);
            const int val5 = (int)(g_analog_mux.GetNormalizedValue(5) * 1023);
            const int val6 = (int)(g_analog_mux.GetNormalizedValue(6) * 1023);
            const int val7 = (int)(g_analog_mux.GetNormalizedValue(7) * 1023);
            
            // **シンプルFMテスト：1つのFMシンセのみ使用**
            for (uint32_t i = 0; i < sample_count; i++) {
#if DEBUG_FALLBACK_SINE
                // フォールバック用サイン波（問題切り分け用）
                static float phase = 0.0f;
                const float freq = 440.0f;
                const float sample_rate = 48000.0f;
                const float amplitude = 0.1f;
                
                mixed_out = amplitude * sinf(phase);
                phase += 2.0f * M_PI * freq / sample_rate;
                if (phase >= 2.0f * M_PI) {
                    phase -= 2.0f * M_PI;
                }
#else
                // **参照版の意図的破綻設計：val0=0で最高音質**
                if (val0 > 0) { // ここは0が一番音が良い気がする
                    out1 = fm1.Process();
                } else {
                    out1 = 0.0f;
                }
                
                if (val3 > 0) {
                    out2 = fm2.Process();
                } else {
                    out2 = 0.0f;
                }

                // ミキシング（平均化）
                mixed_out = (out1 + out2) * 0.5f;

                // **オーバードライブエフェクト（参照版と同じ順序）**
                mixed_out = overdrive.Process(mixed_out);
                
                // ボリューム適用（参照版と完全同じdBスケーリング）
                mixed_out *= dbtoa(scaleValue(val7, 0, 1023, -70.0f, 6.0f));
                
                // クリッピング防止
                if (mixed_out > 1.0f) mixed_out = 1.0f;
                if (mixed_out < -1.0f) mixed_out = -1.0f;
#endif

                // 32bit signed integerに変換
                sample = (int32_t)(mixed_out * 2147483647.0f);
                samples[i * 2 + 0] = sample;  // Left
                samples[i * 2 + 1] = sample;  // Right

                // 出力音のレベルを監視して、一定より小さかったらFMシンセのパラメータをランダムに動かす（参照版完全再現）
                if (fabsf(mixed_out) < 0.01f) {
                    // ランダムな値を生成してFMシンセのパラメータを更新
                    fm1.SetFrequency(100 + (rand() % 900)); // 周波数をランダムに設定
                    fm1.SetIndex(rand() % 20); // インデックスをランダムに設定
                    fm1.SetRatio(1 + (rand() % 19)); // レシオをランダムに設定
                    fm2.SetFrequency(100 + (rand() % 900)); // 周波数をランダムに設定
                    fm2.SetIndex(rand() % 20); // インデックスをランダムに設定
                    fm2.SetRatio(1 + (rand() % 19)); // レシオをランダムに設定
                }

                // **参照版の意図的破綻設計（直接乗算によるクロスモジュレーション）**
                if (i % 2 == 0) {
                    // 1つ目のFMシンセのインデックスとレシオを動的に設定
                    fm1.SetFrequency(scaleValue(val0, 0, 1023, 0.0f, 1000.0f) * out2); // 出力値を基に周波数を設定
                    fm1.SetIndex(scaleValue(val1, 0, 1023, 0.0f, 20.0f) * out2); // 出力値を基にインデックスを設定
                    fm1.SetRatio(scaleValue(val2, 0, 1023, 0.0f, 20.0f) * out2); // 出力値を基にレシオを設定
                    // 2つ目のFMシンセのインデックスとレシオを動的に設定
                    fm2.SetFrequency(scaleValue(val3, 0, 1023, 0.0f, 1000.0f) * out1); // 出力値を基に周波数を設定
                    fm2.SetIndex(scaleValue(val4, 0, 1023, 0.0f, 20.0f) * out1); // 出力値を基にインデックスを設定
                    fm2.SetRatio(scaleValue(val5, 0, 1023, 0.0f, 20.0f) * out1); // 出力値を基にレシオを設定
                    // **オーバードライブのドライブを動的に設定（val6で制御）**
                    overdrive.SetDrive(scaleValue(val6, 0, 1023, 0.0f, 1.0f)); // 出力値を基にドライブを設定
                }
            }
            
            // デバッグ出力（最初の数バッファ）
            buffer_count++;
            if (buffer_count <= 3) {
                printf("FM TEST Buffer %d: sample_count=%d, first_sample=0x%08x, out1=%.4f\n", 
                       buffer_count, sample_count, samples[0], out1);
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
            printf("Cross FM: %d buffers processed\n", buffer_count);
        }
    }
}

/**
 * @brief システム初期化
 */
bool init_synth() {
    stdio_init_all();
    
    // **LEDデバッグ：プログラムが動作していることを視覚的に確認**
    const uint LED_PIN = 25;  // Pico 2の内蔵LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    
    // LEDパターンで起動確認（3回点滅）
    for (int i = 0; i < 3; i++) {
        gpio_put(LED_PIN, 1);
        sleep_ms(200);
        gpio_put(LED_PIN, 0);
        sleep_ms(200);
    }
    
    // USBシリアル接続の確実な確立
    sleep_ms(3000);  // 3秒待機に延長
    
    printf("=== Cross FM Synthesizer DEBUG VERSION v3.0 ===\n");
    printf("Build time: " __DATE__ " " __TIME__ "\n");
    printf("System starting...\n");
    
    // 初期化の各ステップでデバッグ出力
    printf("Step 1: USB Serial established\n");
    
    // LED点灯でStep 1完了を表示
    gpio_put(LED_PIN, 1);
    
    // システムクロック設定 (96MHz動作) - 一時的に無効化してテスト
    printf("Step 2: Skipping system clock reconfiguration for stability\n");
    /*
    pll_init(pll_usb, 1, 1536 * MHZ, 4, 4);
    clock_configure(clk_usb, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 96 * MHZ, 48 * MHZ);
    clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 96 * MHZ, 96 * MHZ);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, 96 * MHZ, 96 * MHZ);
    stdio_init_all();
    */
    
    printf("Step 3: System clock configuration skipped\n");
    
    // DCDC電源制御
    printf("Step 4: Configuring DCDC for low-noise audio...\n");
    const uint32_t PIN_DCDC_PSM_CTRL = 23;
    gpio_init(PIN_DCDC_PSM_CTRL);
    gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
    gpio_put(PIN_DCDC_PSM_CTRL, 1);
    printf("Step 5: DCDC configured\n");
    
    // アナログマルチプレクサー初期化
    printf("Step 6: Initializing analog multiplexer...\n");
    AnalogMux::Config mux_config = {
        .pin_enable = kPinNEnable,
        .pin_s0 = kPinS0,
        .pin_s1 = kPinS1,
        .pin_s2 = kPinS2,
        .adc_pin = kAnalogIn,
        .adc_channel = 0,
        .scan_period_ms = 1, // 参照版と完全同じ（1ms高速スキャン）
        .enable_active_low = true
    };
    g_analog_mux.Init(mux_config);
    printf("Step 7: Analog multiplexer initialized\n");
    
    // オーディオシステム初期化（参照版と同じ48kHz）
    static audio_format_t audio_format = {
        .sample_freq = 48000,
        .pcm_format = AUDIO_PCM_FORMAT_S32,
        .channel_count = AUDIO_CHANNEL_STEREO
    };
    
    static audio_buffer_format_t producer_format = {
        .format = &audio_format,
        .sample_stride = 8
    };
    
    static audio_i2s_config_t i2s_config = {
        .data_pin = PICO_AUDIO_I2S_DATA_PIN,
        .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
        .dma_channel0 = 0,
        .dma_channel1 = 1,
        .pio_sm = 0
    };
    
    printf("I2S Config: data_pin=%d, clock_pin_base=%d\n", 
           i2s_config.data_pin, i2s_config.clock_pin_base);
    
    g_audio_pool = audio_new_producer_pool(&producer_format, 3, SAMPLES_PER_BUFFER);
    if (!g_audio_pool) {
        printf("Failed to create audio buffer pool\n");
        return false;
    }
    printf("Audio buffer pool created successfully\n");
    
    const audio_format_t *output_format = audio_i2s_setup(&audio_format, &audio_format, &i2s_config);
    if (!output_format) {
        printf("PicoAudio: Unable to open audio device.\n");
        return false;
    }
    
    printf("I2S setup successful, output format: freq=%d\n", output_format->sample_freq);
    
    printf("Connecting audio pool to I2S...\n");
    bool connect_result = audio_i2s_connect(g_audio_pool);
    if (!connect_result) {
        printf("Failed to connect audio pool to I2S!\n");
        return false;
    }
    printf("Audio pool connected successfully\n");
    
    // 初期バッファデータ設定
    {
        audio_buffer_t *ab = take_audio_buffer(g_audio_pool, true);
        int32_t *samples = (int32_t *) ab->buffer->bytes;
        for (uint i = 0; i < ab->max_sample_count; i++) {
            samples[i*2+0] = DAC_ZERO;
            samples[i*2+1] = DAC_ZERO;
        }
        ab->sample_count = ab->max_sample_count;
        give_audio_buffer(g_audio_pool, ab);
    }
    
    printf("Enabling I2S output...\n");
    audio_i2s_set_enabled(true);
    printf("I2S output enabled\n");
    
    printf("Launching Core1 audio processing...\n");
    multicore_launch_core1(core1_audio_loop);
    
    sleep_ms(500);
    printf("Enabling audio generation...\n");
    audio_enabled = true;
    
    printf("Cross FM Synthesizer initialized\n");
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
    printf("Knob assignments (reference version with overdrive):\n");
    printf("  val0: FM1 Frequency Base (0-1000Hz) - 0 = BEST SOUND!\n");
    printf("  val1: FM1 Index Base (0-20)\n");
    printf("  val2: FM1 Ratio Base (0-20)\n");
    printf("  val3: FM2 Frequency Base (0-1000Hz)\n");
    printf("  val4: FM2 Index Base (0-20)\n");
    printf("  val5: FM2 Ratio Base (0-20)\n");
    printf("  val6: Overdrive Drive (0.0-1.0)\n");
    printf("  val7: Master Volume (-70dB to +6dB)\n");
    printf("Cross-modulation: FM1 <-> FM2 mutual modulation (intentional chaos!)\n\n");
    
    // メインループ（参照版はArduinoのloop()なので、ここは最小限）
    while (true) {
        // デバッグ情報を定期的に出力
        static uint32_t last_debug_time = 0;
        uint32_t current_time = to_ms_since_boot(get_absolute_time());
        
        if (current_time - last_debug_time > 10000) {  // 10秒ごと
            g_analog_mux.Update();
            printf("Knobs: %d %d %d %d %d %d %d %d\n",
                   (int)(g_analog_mux.GetNormalizedValue(0) * 1023),
                   (int)(g_analog_mux.GetNormalizedValue(1) * 1023),
                   (int)(g_analog_mux.GetNormalizedValue(2) * 1023),
                   (int)(g_analog_mux.GetNormalizedValue(3) * 1023),
                   (int)(g_analog_mux.GetNormalizedValue(4) * 1023),
                   (int)(g_analog_mux.GetNormalizedValue(5) * 1023),
                   (int)(g_analog_mux.GetNormalizedValue(6) * 1023),
                   (int)(g_analog_mux.GetNormalizedValue(7) * 1023));
            last_debug_time = current_time;
        }
        
        sleep_ms(100);
    }
    
    return 0;
}