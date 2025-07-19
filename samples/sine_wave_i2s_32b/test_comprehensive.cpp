/**
 * @file test_comprehensive.cpp
 * @brief I2Sオーディオシステム包括的テストプログラム
 * 
 * このプログラムは段階的にI2Sオーディオシステムをテストし、
 * どの段階で問題が発生するかを正確に特定します。
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
// テスト段階の定義
// =============================================================================

typedef enum {
    TEST_BASIC_INIT = 0,
    TEST_CLOCK_SETUP,
    TEST_AUDIO_FORMAT,
    TEST_BUFFER_POOL,
    TEST_I2S_SETUP,
    TEST_I2S_CONNECT,
    TEST_I2S_ENABLE,
    TEST_AUDIO_GENERATION,
    TEST_COMPLETE
} test_stage_t;

// =============================================================================
// グローバル変数
// =============================================================================

static test_stage_t current_stage = TEST_BASIC_INIT;
static const char* stage_names[] = {
    "基本初期化",
    "クロック設定",
    "オーディオフォーマット設定",
    "バッファプール作成",
    "I2Sハードウェア設定",
    "I2S接続",
    "I2S有効化", 
    "音声データ生成",
    "テスト完了"
};

audio_buffer_pool_t *ap = nullptr;
static bool test_failed = false;

// オーディオ設定
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

// =============================================================================
// テスト用ユーティリティ関数
// =============================================================================

void test_log(const char* message) {
    printf("[%s] %s\n", stage_names[current_stage], message);
}

void test_error(const char* error_msg) {
    printf("❌ エラー [%s]: %s\n", stage_names[current_stage], error_msg);
    test_failed = true;
}

void test_success(const char* success_msg) {
    printf("✅ 成功 [%s]: %s\n", stage_names[current_stage], success_msg);
}

void advance_stage() {
    if (current_stage < TEST_COMPLETE) {
        current_stage = (test_stage_t)(current_stage + 1);
        printf("\n--- 段階 %d: %s ---\n", current_stage, stage_names[current_stage]);
    }
}

// =============================================================================
// 段階的テスト関数
// =============================================================================

bool test_basic_init() {
    test_log("基本システム初期化中...");
    
    stdio_init_all();
    sleep_ms(2000);  // USBシリアル安定化
    
    test_success("基本初期化完了");
    return true;
}

bool test_clock_setup() {
    test_log("システムクロック設定中...");
    
    // USB PLL を 96MHz に設定
    pll_init(pll_usb, 1, 1536 * MHZ, 4, 4);
    test_log("USB PLL設定完了");
    
    // USB クロックを 48MHz に設定
    clock_configure(clk_usb,
        0,
        CLOCKS_CLK_USB_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
        96 * MHZ,
        48 * MHZ);
    test_log("USBクロック設定完了");
        
    // システムクロックを 96MHz に変更
    clock_configure(clk_sys,
        CLOCKS_CLK_SYS_CTRL_SRC_VALUE_CLKSRC_CLK_SYS_AUX,
        CLOCKS_CLK_SYS_CTRL_AUXSRC_VALUE_CLKSRC_PLL_USB,
        96 * MHZ,
        96 * MHZ);
    test_log("システムクロック設定完了");
        
    // 周辺機器クロック設定
    clock_configure(clk_peri,
        0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
        96 * MHZ,
        96 * MHZ);
    test_log("周辺機器クロック設定完了");
    
    // UART再初期化
    stdio_init_all();
    test_log("UART再初期化完了");
    
    test_success("すべてのクロック設定完了");
    return true;
}

bool test_audio_format() {
    test_log("オーディオフォーマット検証中...");
    
    if (audio_format.sample_freq != 44100) {
        test_error("サンプリング周波数が正しくありません");
        return false;
    }
    
    if (audio_format.pcm_format != AUDIO_PCM_FORMAT_S32) {
        test_error("PCMフォーマットが正しくありません");
        return false;
    }
    
    if (audio_format.channel_count != AUDIO_CHANNEL_STEREO) {
        test_error("チャンネル数が正しくありません");
        return false;
    }
    
    test_success("オーディオフォーマット検証完了");
    return true;
}

bool test_buffer_pool() {
    test_log("オーディオバッファプール作成中...");
    
    ap = audio_new_producer_pool(&producer_format, 3, 1156);
    if (!ap) {
        test_error("バッファプール作成に失敗");
        return false;
    }
    
    test_success("バッファプール作成完了");
    return true;
}

bool test_i2s_setup() {
    test_log("I2Sハードウェア設定中...");
    
    const audio_format_t *output_format = audio_i2s_setup(&audio_format, &audio_format, &i2s_config);
    if (!output_format) {
        test_error("I2Sハードウェア設定に失敗");
        return false;
    }
    
    test_success("I2Sハードウェア設定完了");
    return true;
}

bool test_i2s_connect() {
    test_log("I2S接続設定中...");
    
    bool ok = audio_i2s_connect(ap);
    if (!ok) {
        test_error("I2S接続に失敗");
        return false;
    }
    
    test_success("I2S接続完了");
    return true;
}

bool test_i2s_enable() {
    test_log("I2S有効化中...");
    
    // 初期バッファを設定
    audio_buffer_t *buffer = take_audio_buffer(ap, true);
    if (!buffer) {
        test_error("初期バッファの取得に失敗");
        return false;
    }
    
    int32_t *samples = (int32_t *) buffer->buffer->bytes;
    for (uint i = 0; i < buffer->max_sample_count; i++) {
        samples[i*2+0] = 1;  // 左チャンネル
        samples[i*2+1] = 1;  // 右チャンネル
    }
    buffer->sample_count = buffer->max_sample_count;
    give_audio_buffer(ap, buffer);
    
    audio_i2s_set_enabled(true);
    test_success("I2S有効化完了");
    return true;
}

bool test_audio_generation() {
    test_log("音声データ生成テスト中...");
    
    // 10回のバッファ生成をテスト
    for (int i = 0; i < 10; i++) {
        audio_buffer_t *buffer = take_audio_buffer(ap, false);
        if (!buffer) {
            test_log("バッファが利用できません（正常な場合があります）");
            sleep_ms(10);
            continue;
        }
        
        int32_t *samples = (int32_t *) buffer->buffer->bytes;
        for (uint j = 0; j < buffer->max_sample_count; j++) {
            // シンプルなテスト信号
            int32_t value = (i * 1000 + j) << 16;
            samples[j*2+0] = value;  // 左チャンネル
            samples[j*2+1] = value;  // 右チャンネル
        }
        
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(ap, buffer);
        
        printf("バッファ %d 生成完了\n", i + 1);
        sleep_ms(100);
    }
    
    test_success("音声データ生成テスト完了");
    return true;
}

// =============================================================================
// メイン関数
// =============================================================================

int main() {
    printf("\n=== I2Sオーディオシステム包括的テスト ===\n");
    printf("各段階でテストを実行し、問題箇所を特定します\n\n");
    
    // 段階的テスト実行
    printf("--- 段階 %d: %s ---\n", current_stage, stage_names[current_stage]);
    
    if (!test_basic_init()) goto test_end;
    advance_stage();
    
    if (!test_clock_setup()) goto test_end;
    advance_stage();
    
    if (!test_audio_format()) goto test_end;
    advance_stage();
    
    if (!test_buffer_pool()) goto test_end;
    advance_stage();
    
    if (!test_i2s_setup()) goto test_end;
    advance_stage();
    
    if (!test_i2s_connect()) goto test_end;
    advance_stage();
    
    if (!test_i2s_enable()) goto test_end;
    advance_stage();
    
    if (!test_audio_generation()) goto test_end;
    advance_stage();
    
test_end:
    printf("\n=== テスト結果 ===\n");
    if (test_failed) {
        printf("❌ テスト失敗: %s段階で問題が発生しました\n", stage_names[current_stage]);
        printf("この段階のコードをデバッグしてください\n");
    } else {
        printf("✅ 全テスト成功！I2Sオーディオシステムは正常に動作しています\n");
        printf("音声が出力されない場合は、ハードウェア接続を確認してください\n");
        
        // 継続的な音声出力テスト
        printf("\n継続的な音声出力テストを開始します（qキーで終了）\n");
        while (true) {
            int c = getchar_timeout_us(0);
            if (c == 'q') break;
            
            audio_buffer_t *buffer = take_audio_buffer(ap, false);
            if (buffer) {
                int32_t *samples = (int32_t *) buffer->buffer->bytes;
                for (uint i = 0; i < buffer->max_sample_count; i++) {
                    // 440Hz サイン波テスト信号
                    float phase = 2.0f * M_PI * 440.0f * i / 44100.0f;
                    int32_t value = (int32_t)(sinf(phase) * 0x7FFFFFFF);
                    samples[i*2+0] = value;  // 左チャンネル
                    samples[i*2+1] = value;  // 右チャンネル
                }
                buffer->sample_count = buffer->max_sample_count;
                give_audio_buffer(ap, buffer);
            }
            sleep_ms(10);
        }
    }
    
    // クリーンアップ
    if (ap) {
        audio_i2s_set_enabled(false);
        audio_i2s_end();
    }
    
    printf("テスト終了\n");
    return test_failed ? 1 : 0;
}

// DMAコールバック（必要に応じて）
extern "C" {
void i2s_callback_func() {
    // 空の実装
}
}