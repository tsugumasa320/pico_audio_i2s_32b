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

// バッファ設定（参照版と同じ）
#define BUFFER_SIZE 2
#define SAMPLES_PER_BUFFER 1156

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
    
    // DaisySPオブジェクトの初期化
    const float sample_rate = 44100.0f;
    
    fm1.Init(sample_rate);
    fm1.SetFrequency(440);
    fm1.SetRatio(0.5f);
    fm1.SetIndex(100.0f);
    
    fm2.Init(sample_rate);
    fm2.SetFrequency(330);
    fm2.SetRatio(0.33f);
    fm2.SetIndex(50.0f);
    
    overdrive.Init();
    overdrive.SetDrive(0.5f);
    
    dcBlock.Init(sample_rate);
    
    antiAliasFilter1.Init(sample_rate);
    antiAliasFilter1.SetType(LOWPASS);
    antiAliasFilter1.SetCutoff(sample_rate / 2 * 0.9f); // ナイキスト周波数の90%（参照版と同じ）
    antiAliasFilter1.SetQ(1.0f);
    
    antiAliasFilter2.Init(sample_rate);
    antiAliasFilter2.SetType(LOWPASS);
    antiAliasFilter2.SetCutoff(sample_rate / 2 * 0.9f); // ナイキスト周波数の90%（参照版と同じ）
    antiAliasFilter2.SetQ(1.0f);
    
    printf("DaisySP objects initialized\n");
    
    // 参照版の変数
    static float out1, out2, mixed_out;
    
    while (true) {
        audio_buffer_t *buffer = take_audio_buffer(g_audio_pool, true);
        if (!buffer) {
            printf("Failed to get audio buffer!\n");
            continue;
        }

        int32_t *samples = (int32_t *)buffer->buffer->bytes;
        const uint32_t sample_count = buffer->max_sample_count;

        if (audio_enabled) {
            // アナログマルチプレクサーの値を取得
            g_analog_mux.Update();
            const int val0 = (int)(g_analog_mux.GetNormalizedValue(0) * 1023);
            const int val1 = (int)(g_analog_mux.GetNormalizedValue(1) * 1023);
            const int val2 = (int)(g_analog_mux.GetNormalizedValue(2) * 1023);
            const int val3 = (int)(g_analog_mux.GetNormalizedValue(3) * 1023);
            const int val4 = (int)(g_analog_mux.GetNormalizedValue(4) * 1023);
            const int val5 = (int)(g_analog_mux.GetNormalizedValue(5) * 1023);
            const int val6 = (int)(g_analog_mux.GetNormalizedValue(6) * 1023);
            const int val7 = (int)(g_analog_mux.GetNormalizedValue(7) * 1023);
            
            // 参照版と同じ処理：BUFFER_SIZE分のサンプルを生成
            for (int s = 0; s < BUFFER_SIZE && s < sample_count; s++) {
                // 参照版の条件分岐（FM1 ON/OFF）
                if (val0 > 0) { 
                    out1 = fm1.Process();
                } else {
                    out1 = 0.0f;
                }
                
                // 参照版の条件分岐（FM2 ON/OFF）
                if (val3 > 0) {
                    out2 = fm2.Process();
                } else {
                    out2 = 0.0f;
                }

                // ミキシング（平均化）
                mixed_out = (out1 + out2) * 0.5f;

                // エフェクトチェーン（参照版と同じ順序）
                mixed_out = overdrive.Process(mixed_out);
                mixed_out = antiAliasFilter1.Process(mixed_out);
                mixed_out = antiAliasFilter2.Process(mixed_out);
                mixed_out = dcBlock.Process(mixed_out);
                mixed_out = daisysp::fclamp(mixed_out, -1.0f, 1.0f);
                
                // ボリューム適用（参照版と同じdB範囲）
                mixed_out *= dbtoa(scaleValue(val7, 0, 1023, -70.0f, 6.0f));

                // 32bit signed integerに変換
                int32_t sample_value = (int32_t)(mixed_out * 0x7FFFFF00);
                
                // ステレオ出力
                if (s < sample_count) {
                    samples[s * 2 + 0] = sample_value;  // Left
                    samples[s * 2 + 1] = sample_value;  // Right
                }

                // 参照版のクロスモジュレーション処理（2サンプルごと）
                if (s % 2 == 0) {
                    // 1つ目のFMシンセをout2で変調（参照版通り生の値を使用）
                    fm1.SetFrequency(scaleValue(val0, 0, 1023, 0.0f, 1000.0f) * out2);
                    fm1.SetIndex(scaleValue(val1, 0, 1023, 0.0f, 20.0f) * out2);
                    fm1.SetRatio(scaleValue(val2, 0, 1023, 0.0f, 20.0f) * out2);
                    
                    // 2つ目のFMシンセをout1で変調（参照版通り生の値を使用）
                    fm2.SetFrequency(scaleValue(val3, 0, 1023, 0.0f, 1000.0f) * out1);
                    fm2.SetIndex(scaleValue(val4, 0, 1023, 0.0f, 20.0f) * out1);
                    fm2.SetRatio(scaleValue(val5, 0, 1023, 0.0f, 20.0f) * out1);
                    
                    // オーバードライブのドライブ設定
                    overdrive.SetDrive(scaleValue(val6, 0, 1023, 0.0f, 1.0f));
                }

                // 出力音のレベルが小さい場合のランダム処理（参照版と同じ）
                if (fabsf(mixed_out) < 0.01f) {
                    fm1.SetFrequency(100 + (rand() % 900)); // 100-1000Hz
                    fm1.SetIndex(rand() % 20); // 0-20
                    fm1.SetRatio(1 + (rand() % 19)); // 1-20
                    fm2.SetFrequency(100 + (rand() % 900));
                    fm2.SetIndex(rand() % 20);
                    fm2.SetRatio(1 + (rand() % 19));
                }
            }
            
            // デバッグ出力（最初の数バッファ）
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
            printf("Cross FM: %d buffers processed\n", buffer_count);
        }
    }
}

/**
 * @brief システム初期化
 */
bool init_synth() {
    stdio_init_all();
    
    printf("=== Cross FM Synthesizer (Reference Version) ===\n");
    printf("Initializing system...\n");
    
    // USBシリアル安定化のための待機
    sleep_ms(2000);
    
    // システムクロック設定 (96MHz動作)
    printf("Setting up system clock to 96MHz...\n");
    
    pll_init(pll_usb, 1, 1536 * MHZ, 4, 4);
    clock_configure(clk_usb, 0, CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 96 * MHZ, 48 * MHZ);
    clock_configure(clk_sys, CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX, CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB, 96 * MHZ, 96 * MHZ);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, 96 * MHZ, 96 * MHZ);
    stdio_init_all();
    
    printf("System clock configured to 96MHz\n");
    
    // DCDC電源制御
    const uint32_t PIN_DCDC_PSM_CTRL = 23;
    gpio_init(PIN_DCDC_PSM_CTRL);
    gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
    gpio_put(PIN_DCDC_PSM_CTRL, 1);
    printf("DCDC configured for low-noise audio\n");
    
    // アナログマルチプレクサー初期化
    printf("Initializing analog multiplexer...\n");
    AnalogMux::Config mux_config = {
        .pin_enable = kPinNEnable,
        .pin_s0 = kPinS0,
        .pin_s1 = kPinS1,
        .pin_s2 = kPinS2,
        .adc_pin = kAnalogIn,
        .adc_channel = 0,
        .scan_period_ms = 1, // 参照版と同じ高速スキャン
        .enable_active_low = true
    };
    g_analog_mux.Init(mux_config);
    printf("Analog multiplexer initialized\n");
    
    // オーディオシステム初期化
    static audio_format_t audio_format = {
        .sample_freq = 44100,
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
    
    printf("Cross FM Synthesizer starting...\n");
    printf("Knob assignments (same as reference):\n");
    printf("  val0: FM1 Frequency Base (0-1000Hz)\n");
    printf("  val1: FM1 Index Base (0-20)\n");
    printf("  val2: FM1 Ratio Base (0-20)\n");
    printf("  val3: FM2 Frequency Base (0-1000Hz)\n");
    printf("  val4: FM2 Index Base (0-20)\n");
    printf("  val5: FM2 Ratio Base (0-20)\n");
    printf("  val6: Overdrive Drive (0.0-1.0)\n");
    printf("  val7: Master Volume (-70dB to +6dB)\n");
    printf("Cross-modulation: FM1 modulated by FM2 output, FM2 modulated by FM1 output\n\n");
    
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