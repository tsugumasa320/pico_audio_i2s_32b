/**
 * @file sine_wave.cpp
 * @brief 32bit I2S DAC サイン波生成サンプル
 * 
 * このサンプルは、Raspberry Pi Pico/Pico 2 で 32bit I2S DAC を使用して
 * インタラクティブなデュアルチャンネルサイン波ジェネレーターを実装します。
 * 
 * 機能:
 * - 左右独立した周波数制御
 * - リアルタイム音量調整
 * - キーボードによるインタラクティブ制御
 * - 32bit高精度音声出力
 * 
 * 操作方法:
 * - "+"/"=": 音量アップ
 * - "-": 音量ダウン
 * - "["/"]": 左チャンネル周波数調整
 * - "{"/"}": 右チャンネル周波数調整
 * - "q": 終了
 * 
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <math.h>

#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/clocks.h"
#include "hardware/pio.h"

#include "pico/stdlib.h"
#include "pico/audio.h"
#include "pico/audio_i2s.h"
#include "analog_mux.h"

// =============================================================================
// 定数定義
// =============================================================================

#define SINE_WAVE_TABLE_LEN 2048        // サイン波テーブルのサンプル数
#define SAMPLES_PER_BUFFER 1156          // バッファあたりのサンプル数（チャンネルあたり）

static const uint32_t PIN_DCDC_PSM_CTRL = 23;  // DCDC PSM制御ピン（ノイズ低減用）

// =============================================================================
// グローバル変数
// =============================================================================

audio_buffer_pool_t *ap;               // オーディオバッファプール
static bool decode_flg = false;        // 音声生成フラグ
static constexpr int32_t DAC_ZERO = 1; // DAC出力のゼロレベル

#define audio_pio __CONCAT(pio, PICO_AUDIO_I2S_PIO)

// =============================================================================
// オーディオ設定
// =============================================================================

/** オーディオフォーマット設定 */
static audio_format_t audio_format = {
    .sample_freq = 44100,                // サンプリング周波数: 44.1kHz (CD品質)
    .pcm_format = AUDIO_PCM_FORMAT_S32,  // PCMフォーマット: 32bit signed
    .channel_count = AUDIO_CHANNEL_STEREO // チャンネル数: ステレオ (2ch)
};

/** バッファフォーマット設定 */
static audio_buffer_format_t producer_format = {
    .format = &audio_format,
    .sample_stride = 8                   // ステレオ32bitなので8バイト/サンプル
};

/** I2S設定 */
static audio_i2s_config_t i2s_config = {
    .data_pin = PICO_AUDIO_I2S_DATA_PIN,         // データピン (デフォルト: GP18)
    .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE, // クロックピンベース (デフォルト: GP16)
    .dma_channel0 = 0,                           // DMAチャンネル0
    .dma_channel1 = 1,                           // DMAチャンネル1
    .pio_sm = 0                                  // PIOステートマシン番号
};

// =============================================================================
// サイン波生成用変数
// =============================================================================

static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN]; // プリ計算されたサイン波テーブル

// 周波数制御用のステップ値（固定小数点形式）
uint32_t step0 = 0x200000;  // 左チャンネルの周波数ステップ
uint32_t step1 = 0x200000;  // 右チャンネルの周波数ステップ

// 波形の位相（固定小数点形式）
uint32_t pos0 = 0;          // 左チャンネルの位相
uint32_t pos1 = 0;          // 右チャンネルの位相

const uint32_t pos_max = 0x10000 * SINE_WAVE_TABLE_LEN; // 位相の最大値
uint vol = 8;               // 音量レベル（0-256）- 歪み防止のため低減

// アナログマルチプレクサー設定
static AnalogMux g_analog_mux;
enum {
    kPinNEnable = 0,  // Enable pin (active low) - 74HC4051の/ENピン
    kPinS0      = 3,  // Select pin S0 - 74HC4051のS0ピン
    kPinS1      = 2,  // Select pin S1 - 74HC4051のS1ピン
    kPinS2      = 1,  // Select pin S2 - 74HC4051のS2ピン
    kAnalogIn   = 26, // ADC input pin - 74HC4051のCOMピンからの入力
};

// ノブ機能の定義
enum KnobFunction {
    KNOB_VOLUME = 0,     // ノブ0: マスターボリューム
    KNOB_LEFT_FREQ = 1,  // ノブ1: 左チャンネル周波数
    KNOB_RIGHT_FREQ = 2, // ノブ2: 右チャンネル周波数
    KNOB_UNUSED3 = 3,    // ノブ3: 未使用
    KNOB_UNUSED4 = 4,    // ノブ4: 未使用
    KNOB_UNUSED5 = 5,    // ノブ5: 未使用
    KNOB_UNUSED6 = 6,    // ノブ6: 未使用
    KNOB_UNUSED7 = 7,    // ノブ7: 未使用
};"}

// =============================================================================
// ユーティリティ関数
// =============================================================================

/**
 * @brief 起動からの経過時間をミリ秒で取得
 * @return ミリ秒単位の経過時間
 */
static inline uint32_t _millis(void)
{
	return to_ms_since_boot(get_absolute_time());
}

/**
 * @brief I2Sオーディオシステムの終了処理
 * 
 * オーディオ出力を停止し、使用していたバッファとリソースを
 * すべて解放します。
 */
void i2s_audio_deinit()
{
    decode_flg = false;  // 音声生成を停止

    // I2S出力を無効化して終了
    audio_i2s_set_enabled(false);
    audio_i2s_end();

    // すべてのオーディオバッファを解放
    audio_buffer_t* ab;
    
    // 使用中のバッファを解放
    ab = take_audio_buffer(ap, false);
    while (ab != nullptr) {
        free(ab->buffer->bytes);
        free(ab->buffer);
        ab = take_audio_buffer(ap, false);
    }
    
    // 空きバッファを解放
    ab = get_free_audio_buffer(ap, false);
    while (ab != nullptr) {
        free(ab->buffer->bytes);
        free(ab->buffer);
        ab = get_free_audio_buffer(ap, false);
    }
    
    // フルバッファを解放
    ab = get_full_audio_buffer(ap, false);
    while (ab != nullptr) {
        free(ab->buffer->bytes);
        free(ab->buffer);
        ab = get_full_audio_buffer(ap, false);
    }
    
    // バッファプール自体を解放
    free(ap);
    ap = nullptr;
}

/**
 * @brief I2Sオーディオシステムの初期化
 * 
 * 指定されたサンプリング周波数でI2Sオーディオシステムを初期化し、
 * バッファプールを作成してストリーミングを開始します。
 * 
 * @param sample_freq サンプリング周波数 (Hz)
 * @return 初期化されたオーディオバッファプール
 */
audio_buffer_pool_t *i2s_audio_init(uint32_t sample_freq)
{
    // サンプリング周波数を設定
    audio_format.sample_freq = sample_freq;

    // オーディオバッファプールを作成
    // 3つのバッファを使用してトリプルバッファリングを実現
    audio_buffer_pool_t *producer_pool = audio_new_producer_pool(&producer_format, 3, SAMPLES_PER_BUFFER);
    ap = producer_pool;

    bool __unused ok;
    const audio_format_t *output_format;

    // I2Sハードウェアをセットアップ
    output_format = audio_i2s_setup(&audio_format, &audio_format, &i2s_config);
    if (!output_format) {
        panic("PicoAudio: Unable to open audio device.\n");
    }

    // バッファプールをI2S出力に接続
    ok = audio_i2s_connect(producer_pool);
    assert(ok);
    
    // 初期バッファデータを設定（無音状態）
    {
        audio_buffer_t *ab = take_audio_buffer(producer_pool, true);
        int32_t *samples = (int32_t *) ab->buffer->bytes;
        for (uint i = 0; i < ab->max_sample_count; i++) {
            samples[i*2+0] = DAC_ZERO;  // 左チャンネル
            samples[i*2+1] = DAC_ZERO;  // 右チャンネル
        }
        ab->sample_count = ab->max_sample_count;
        give_audio_buffer(producer_pool, ab);
    }
    
    // I2S出力を有効化
    audio_i2s_set_enabled(true);

    // 音声生成を開始
    decode_flg = true;
    return producer_pool;
}

/**
 * @brief メイン関数
 * 
 * システムを初期化し、インタラクティブなサイン波ジェネレーターを実行します。
 * ユーザーはキーボード入力で音量と周波数をリアルタイムに制御できます。
 */
int main() {

    stdio_init_all();
    
    // デバッグ用の起動確認
    sleep_ms(2000);  // USBシリアル安定化のための待機
    
    printf("\n=== 32bit I2S DAC サイン波ジェネレーター ===\n");
    printf("プログラム開始 - デバッグモード\n");
    printf("操作方法:\n");
    printf("  +/= : 音量アップ\n");
    printf("  -   : 音量ダウン\n");
    printf("  [/] : 左チャンネル周波数調整\n");
    printf("  {/} : 右チャンネル周波数調整\n");
    printf("  q   : 終了\n\n");

    // =============================================================================
    // システムクロック設定 (96MHz動作)
    // =============================================================================
    
    // USB PLL を 96MHz に設定
    pll_init(pll_usb, 1, 1536 * MHZ, 4, 4);
    
    // USB クロックを 48MHz に設定
    clock_configure(clk_usb,
        0,
        CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
        96 * MHZ,
        48 * MHZ);
        
    // システムクロックを 96MHz に変更（高精度オーディオのため）
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

    // =============================================================================
    // DCDC電源制御（オーディオノイズ低減）
    // =============================================================================
    
    // DCDC PSM制御
    // 0: PFM mode (効率重視)
    // 1: PWM mode (リップル改善、オーディオノイズ低減)
    gpio_init(PIN_DCDC_PSM_CTRL);
    gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
    gpio_put(PIN_DCDC_PSM_CTRL, 1); // PWMモードでオーディオノイズを低減
    
    // アナログマルチプレクサー初期化
    printf("アナログマルチプレクサー初期化中...\n");
    AnalogMux::Config mux_config = {
        .pin_enable = kPinNEnable,
        .pin_s0 = kPinS0,
        .pin_s1 = kPinS1,
        .pin_s2 = kPinS2,
        .adc_pin = kAnalogIn,
        .adc_channel = 0,  // ADC channel 0 for pin 26
        .scan_period_ms = 10,
        .enable_active_low = true
    };
    g_analog_mux.Init(mux_config);
    printf("アナログマルチプレクサー初期化完了\n");

    // =============================================================================
    // サイン波テーブルの生成
    // =============================================================================
    
    // プリ計算されたサイン波テーブルを生成（高速な波形生成のため）
    for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
        sine_wave_table[i] = 32767 * cosf(i * 2 * (float) (M_PI / SINE_WAVE_TABLE_LEN));
    }

    printf("I2Sオーディオシステム初期化中...\n");
    
    // I2Sオーディオシステムを 44.1kHz で初期化
    ap = i2s_audio_init(44100);
    
    printf("初期化完了。音声出力を開始しました。\n\n");

    // =============================================================================
    // メインループ（インタラクティブ制御）
    // =============================================================================
    
    while (true) {
        // アナログマルチプレクサーの値を更新
        g_analog_mux.Update();
        
        // ノブの値を取得 (0.0-1.0に正規化済み)
        float knob_volume = g_analog_mux.GetNormalizedValue(KNOB_VOLUME);     // 音量制御
        float knob_left_freq = g_analog_mux.GetNormalizedValue(KNOB_LEFT_FREQ);  // 左チャンネル周波数
        float knob_right_freq = g_analog_mux.GetNormalizedValue(KNOB_RIGHT_FREQ); // 右チャンネル周波数
        
        // パラメーターをマッピング
        static uint32_t last_update_time = 0;
        uint32_t current_time = _millis();
        
        if (current_time - last_update_time > 50) {  // 50msごとに更新
            // 音量: 0-32の範囲にマッピング（歪み防止のため上限制限）
            uint new_vol = (uint)(knob_volume * 32);
            
            // 左チャンネル周波数: 100Hz-2000Hz相当の範囲
            uint32_t new_step0 = 0x10000 + (uint32_t)(knob_left_freq * (0x200000 - 0x10000));
            
            // 右チャンネル周波数: 100Hz-2000Hz相当の範囲
            uint32_t new_step1 = 0x10000 + (uint32_t)(knob_right_freq * (0x200000 - 0x10000));
            
            // 値を更新（変化があった場合のみ）
            if (abs((int)vol - (int)new_vol) > 1) {
                vol = new_vol;
            }
            
            if (abs((int)(step0 >> 16) - (int)(new_step0 >> 16)) > 10) {
                step0 = new_step0;
            }
            
            if (abs((int)(step1 >> 16) - (int)(new_step1 >> 16)) > 10) {
                step1 = new_step1;
            }
            
            last_update_time = current_time;
        }
        
        // キーボード入力処理（デバッグ用）
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            // 手動制御
            if (c == '-' && vol) vol--;
            if ((c == '=' || c == '+') && vol < 256) vol++;
            if (c == '[' && step0 > 0x10000) step0 -= 0x10000;
            if (c == ']' && step0 < (SINE_WAVE_TABLE_LEN / 16) * 0x20000) step0 += 0x10000;
            if (c == '{' && step1 > 0x10000) step1 -= 0x10000;
            if (c == '}' && step1 < (SINE_WAVE_TABLE_LEN / 16) * 0x20000) step1 += 0x10000;
            if (c == 'q') break;
        }
        
        // 5秒ごとに現在の設定値を表示
        static uint32_t last_debug_time = 0;
        if (current_time - last_debug_time > 5000) {
            printf("Knobs: Vol=%.2f(=%d) L=%.2f(=%d) R=%.2f(=%d)\n", 
                   knob_volume, vol, knob_left_freq, (int)(step0 >> 16), knob_right_freq, (int)(step1 >> 16));
            last_debug_time = current_time;
        }
        
        // 短い待機
        sleep_ms(10);
    }
    
    printf("\n\nシャットダウン中...\n");
    i2s_audio_deinit();  // リソースをクリーンアップ
    printf("終了しました。\n");
    return 0;
}

/**
 * @brief オーディオサンプルの生成とバッファへの書き込み
 * 
 * サイン波テーブルを使用して左右チャンネルのオーディオデータを生成し、
 * 32bit PCMフォーマットでバッファに書き込みます。
 * 
 * 処理内容:
 * 1. 空きバッファの取得
 * 2. 各サンプルに対してサイン波値の計算
 * 3. 音量調整と32bit フルスケール変換
 * 4. 位相の更新とラップアラウンド処理
 * 5. バッファの返却
 */
void decode()
{
    // 空きバッファを取得（ノンブロッキング）
    audio_buffer_t *buffer = take_audio_buffer(ap, false);
    if (buffer == NULL) { 
        return;  // バッファが利用できない場合は何もしない
    }
    
    // バッファを32bitサンプル配列としてアクセス
    int32_t *samples = (int32_t *) buffer->buffer->bytes;
    
    // 各サンプルを生成
    for (uint i = 0; i < buffer->max_sample_count; i++) {
        // サイン波テーブルから値を取得し、音量を適用
        int32_t value0 = (vol * sine_wave_table[pos0 >> 16u]) << 8u;  // 左チャンネル
        int32_t value1 = (vol * sine_wave_table[pos1 >> 16u]) << 8u;  // 右チャンネル
        
        // 32bitフルスケールに変換（ディザリング効果も含む）
        samples[i*2+0] = value0 + (value0 >> 16u);  // 左チャンネル出力
        samples[i*2+1] = value1 + (value1 >> 16u);  // 右チャンネル出力
        
        // 位相を進める
        pos0 += step0;  // 左チャンネルの位相更新
        pos1 += step1;  // 右チャンネルの位相更新
        
        // 位相のラップアラウンド処理（0〜pos_max の範囲を維持）
        if (pos0 >= pos_max) pos0 -= pos_max;
        if (pos1 >= pos_max) pos1 -= pos_max;
    }
    
    // サンプル数を設定してバッファを返却
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(ap, buffer);
    return;
}

// =============================================================================
// I2S DMA割り込みコールバック
// =============================================================================

extern "C" {
/**
 * @brief I2S DMA割り込みハンドラから呼び出されるコールバック関数
 * 
 * この関数は audio_i2s.c 内の audio_i2s_dma_irq_handler() から
 * 呼び出されます。__attribute__((weak)) で宣言されているため、
 * ここで実装を提供することで独自の音声生成処理を実行できます。
 * 
 * 呼び出しタイミング:
 * - DMAバッファが空になったとき
 * - 新しいオーディオデータが必要なとき
 * 
 * 注意事項:
 * - この関数は割り込みコンテキストで実行されます
 * - 処理時間を最小限に抑える必要があります
 * - ブロッキング操作は避けてください
 */
void i2s_callback_func()
{
    if (decode_flg) {
        decode();  // 音声データ生成処理を呼び出し
    }
}
}