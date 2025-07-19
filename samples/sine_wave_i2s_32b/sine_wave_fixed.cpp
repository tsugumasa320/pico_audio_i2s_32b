/**
 * @file sine_wave_fixed.cpp
 * @brief 修正されたサイン波生成プログラム
 * 
 * 歪みの原因を修正し、正確なサイン波を生成します。
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

// =============================================================================
// 設定定数
// =============================================================================

#define SAMPLE_RATE 44100
#define BUFFER_SAMPLES 1156
#define TEST_FREQUENCY 440.0f  // A4音階

// =============================================================================
// グローバル変数
// =============================================================================

audio_buffer_pool_t *ap = nullptr;
static volatile bool audio_enabled = false;

// サイン波生成用の位相累積器（連続的な位相を保持）
static float phase_accumulator = 0.0f;
static const float phase_increment = 2.0f * M_PI * TEST_FREQUENCY / SAMPLE_RATE;

// オーディオ設定
static audio_format_t audio_format = {
    .sample_freq = SAMPLE_RATE,
    .pcm_format = AUDIO_PCM_FORMAT_S32,
    .channel_count = AUDIO_CHANNEL_STEREO
};

static audio_buffer_format_t producer_format = {
    .format = &audio_format,
    .sample_stride = 8  // ステレオ32bit = 8バイト/サンプル
};

static audio_i2s_config_t i2s_config = {
    .data_pin = PICO_AUDIO_I2S_DATA_PIN,
    .clock_pin_base = PICO_AUDIO_I2S_CLOCK_PIN_BASE,
    .dma_channel0 = 0,
    .dma_channel1 = 1,
    .pio_sm = 0
};

// =============================================================================
// 修正されたサイン波生成関数
// =============================================================================

/**
 * @brief 連続的な位相を持つサイン波バッファを生成
 * 
 * バッファ間で位相の連続性を保持することで、クリックノイズや
 * 歪みを防ぎます。
 */
void generate_sine_buffer(audio_buffer_t *buffer) {
    int32_t *samples = (int32_t *) buffer->buffer->bytes;
    
    for (uint i = 0; i < buffer->max_sample_count; i++) {
        // 連続的な位相でサイン波を計算
        float sine_value = sinf(phase_accumulator);
        
        // 32bit PCMの範囲に正規化（少し小さめにして歪みを防ぐ）
        int32_t sample_value = (int32_t)(sine_value * 0x60000000);  // 75%の音量
        
        // ステレオ出力（両チャンネル同じ値）
        samples[i*2+0] = sample_value;  // 左チャンネル
        samples[i*2+1] = sample_value;  // 右チャンネル
        
        // 位相を進める
        phase_accumulator += phase_increment;
        
        // 位相のラップアラウンド（2πで巻き戻し）
        if (phase_accumulator >= 2.0f * M_PI) {
            phase_accumulator -= 2.0f * M_PI;
        }
    }
    
    buffer->sample_count = buffer->max_sample_count;
}

// =============================================================================
// システム初期化
// =============================================================================

bool setup_clocks() {
    printf("システムクロック設定中...\n");
    
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
        
    // 周辺機器クロック設定
    clock_configure(clk_peri,
        0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
        96 * MHZ,
        96 * MHZ);
    
    // UART再初期化
    stdio_init_all();
    
    printf("クロック設定完了\n");
    return true;
}

bool setup_audio() {
    printf("I2Sオーディオシステム初期化中...\n");
    
    // バッファプール作成
    ap = audio_new_producer_pool(&producer_format, 3, BUFFER_SAMPLES);
    if (!ap) {
        printf("❌ バッファプール作成に失敗\n");
        return false;
    }
    printf("✅ バッファプール作成完了\n");
    
    // I2Sハードウェア設定
    const audio_format_t *output_format = audio_i2s_setup(&audio_format, &audio_format, &i2s_config);
    if (!output_format) {
        printf("❌ I2Sハードウェア設定に失敗\n");
        return false;
    }
    printf("✅ I2Sハードウェア設定完了\n");
    
    // I2S接続
    bool ok = audio_i2s_connect(ap);
    if (!ok) {
        printf("❌ I2S接続に失敗\n");
        return false;
    }
    printf("✅ I2S接続完了\n");
    
    // 初期バッファを設定（無音）
    audio_buffer_t *buffer = take_audio_buffer(ap, true);
    if (buffer) {
        int32_t *samples = (int32_t *) buffer->buffer->bytes;
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            samples[i*2+0] = 0;  // 左チャンネル
            samples[i*2+1] = 0;  // 右チャンネル
        }
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
    }
    
    // I2S有効化
    audio_i2s_set_enabled(true);
    audio_enabled = true;
    
    printf("✅ I2Sオーディオシステム初期化完了\n");
    return true;
}

// =============================================================================
// メイン関数
// =============================================================================

int main() {
    stdio_init_all();
    sleep_ms(2000);  // USBシリアル安定化
    
    printf("\n=== 修正されたサイン波生成プログラム ===\n");
    printf("440Hz サイン波を生成します\n");
    printf("キーを押すと停止します\n\n");
    
    // システム初期化
    if (!setup_clocks()) {
        printf("❌ クロック設定に失敗\n");
        return 1;
    }
    
    if (!setup_audio()) {
        printf("❌ オーディオ初期化に失敗\n");
        return 1;
    }
    
    printf("🎵 サイン波出力開始\n");
    printf("位相増分: %.6f rad/sample\n", phase_increment);
    printf("バッファサイズ: %d samples\n", BUFFER_SAMPLES);
    
    uint32_t buffer_count = 0;
    
    // メインループ：連続的なサイン波生成
    while (audio_enabled) {
        // キー入力チェック（ノンブロッキング）
        int c = getchar_timeout_us(0);
        if (c >= 0) {
            printf("\n停止要求を受信\n");
            break;
        }
        
        // 空きバッファを取得
        audio_buffer_t *buffer = take_audio_buffer(ap, false);
        if (buffer) {
            // 連続的なサイン波を生成
            generate_sine_buffer(buffer);
            give_audio_buffer(ap, buffer);
            
            buffer_count++;
            if (buffer_count % 100 == 0) {
                printf("バッファ %u 生成完了 (位相: %.3f)\n", 
                       buffer_count, phase_accumulator);
            }
        } else {
            // バッファが利用できない場合は少し待機
            sleep_ms(1);
        }
    }
    
    // クリーンアップ
    printf("\n🔇 オーディオ停止中...\n");
    audio_enabled = false;
    
    if (ap) {
        audio_i2s_set_enabled(false);
        audio_i2s_end();
    }
    
    printf("プログラム終了\n");
    return 0;
}

// =============================================================================
// DMAコールバック（必要に応じて）
// =============================================================================

extern "C" {
void i2s_callback_func() {
    // 必要に応じてここでリアルタイム処理
    // 現在は空の実装
}
}