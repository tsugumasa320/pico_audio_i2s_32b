# API リファレンス

## 📋 目次
- [概要](#概要)
- [基本的な使用方法](#基本的な使用方法)
- [API 関数](#api-関数)
- [設定マクロ](#設定マクロ)
- [データ構造](#データ構造)
- [使用例](#使用例)

## 🎯 概要

pico_audio_i2s_32b ライブラリは、Raspberry Pi Pico/Pico 2 で 32bit I2S DAC を制御するためのライブラリです。

### 主な特徴
- **32bit ステレオ音声出力**
- **最大 192 KHz サンプリング周波数**
- **PIO (Programmable I/O) ベースの高精度タイミング**
- **DMA による効率的なデータ転送**
- **デュアルコア対応**

## 🚀 基本的な使用方法

### 1. ヘッダーファイルのインクルード
```c
#include "pico/audio_i2s.h"
#include "pico/audio.h"
```

### 2. 基本的な初期化とストリーミング
```c
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/audio.h"

// オーディオフォーマットの設定
audio_format_t audio_format = {
    .sample_freq = 44100,      // サンプリング周波数 (Hz)
    .format = AUDIO_BUFFER_FORMAT_PCM_S32,  // 32bit signed PCM
    .channel_count = 2         // ステレオ
};

// I2S設定
struct audio_i2s_config config = audio_i2s_default_config();

// 初期化
audio_i2s_setup(&audio_format, &config);

// オーディオバッファプールの初期化
audio_buffer_pool_t *producer_pool = audio_new_producer_pool(&audio_format, 2, 1024);

// ストリーミング開始
bool __unused ok = audio_i2s_connect(producer_pool);
audio_i2s_set_enabled(true);
```

## 📚 API 関数

### 初期化関数

#### `audio_i2s_setup()`
```c
bool audio_i2s_setup(const audio_format_t *format, const struct audio_i2s_config *config);
```
I2S オーディオシステムを初期化します。

**パラメータ:**
- `format`: オーディオフォーマット設定
- `config`: I2S 設定

**戻り値:**
- `true`: 成功
- `false`: 失敗

**例:**
```c
audio_format_t format = {
    .sample_freq = 48000,
    .format = AUDIO_BUFFER_FORMAT_PCM_S32,
    .channel_count = 2
};
struct audio_i2s_config config = audio_i2s_default_config();
if (!audio_i2s_setup(&format, &config)) {
    printf("I2S setup failed\n");
}
```

#### `audio_i2s_default_config()`
```c
struct audio_i2s_config audio_i2s_default_config(void);
```
デフォルトの I2S 設定を取得します。

**戻り値:**
- デフォルト設定構造体

### 制御関数

#### `audio_i2s_connect()`
```c
bool audio_i2s_connect(audio_buffer_pool_t *producer_pool);
```
オーディオバッファプールを I2S に接続します。

**パラメータ:**
- `producer_pool`: オーディオデータを供給するバッファプール

**戻り値:**
- `true`: 成功
- `false`: 失敗

#### `audio_i2s_set_enabled()`
```c
void audio_i2s_set_enabled(bool enabled);
```
I2S 出力の有効/無効を切り替えます。

**パラメータ:**
- `enabled`: `true` で有効、`false` で無効

#### `audio_i2s_set_frequency()`
```c
void audio_i2s_set_frequency(uint32_t sample_freq);
```
サンプリング周波数を動的に変更します。

**パラメータ:**
- `sample_freq`: 新しいサンプリング周波数 (Hz)

**例:**
```c
// 44.1 KHz から 48 KHz に変更
audio_i2s_set_frequency(48000);
```

### バッファ管理関数

#### `audio_new_producer_pool()`
```c
audio_buffer_pool_t *audio_new_producer_pool(const audio_format_t *format, 
                                             uint buffer_count, 
                                             uint samples_per_buffer);
```
オーディオデータ生成用のバッファプールを作成します。

**パラメータ:**
- `format`: オーディオフォーマット
- `buffer_count`: バッファ数
- `samples_per_buffer`: バッファあたりのサンプル数

**戻り値:**
- バッファプールへのポインタ

#### `take_audio_buffer()`
```c
audio_buffer_t *take_audio_buffer(audio_buffer_pool_t *pool, bool block);
```
バッファプールから空のバッファを取得します。

**パラメータ:**
- `pool`: バッファプール
- `block`: `true` でブロッキング、`false` でノンブロッキング

**戻り値:**
- 使用可能なバッファ、または `NULL`

#### `give_audio_buffer()`
```c
void give_audio_buffer(audio_buffer_pool_t *pool, audio_buffer_t *buffer);
```
データを書き込んだバッファをプールに返します。

**パラメータ:**
- `pool`: バッファプール
- `buffer`: データが入ったバッファ

### データ変換関数

#### `audio_24_to_32_samples()`
```c
void audio_24_to_32_samples(const int32_t *src, int32_t *dest, uint sample_count);
```
24bit データを 32bit にゼロパディングします。

#### `audio_16_to_32_samples()`
```c
void audio_16_to_32_samples(const int16_t *src, int32_t *dest, uint sample_count);
```
16bit データを 32bit に変換します。

## ⚙️ 設定マクロ

以下のマクロで I2S ピン配置をカスタマイズできます：

### ピン設定
```c
// デフォルト値
#define PICO_AUDIO_I2S_DATA_PIN 18           // I2S データピン
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 16     // I2S クロックピンのベース (BCK=16, LRCK=17)
#define PICO_AUDIO_I2S_PIO 0                 // 使用する PIO インスタンス
#define PICO_AUDIO_I2S_DMA_IRQ 0             // DMA IRQ チャンネル
```

### 機能設定
```c
// デュアルコア処理を有効にする場合
#define CORE1_PROCESS_I2S_CALLBACK
```

## 📊 データ構造

### `audio_format_t`
```c
typedef struct {
    uint32_t sample_freq;        // サンプリング周波数 (Hz)
    audio_format_t format;       // データフォーマット
    uint8_t channel_count;       // チャンネル数
} audio_format_t;
```

### `audio_i2s_config`
```c
struct audio_i2s_config {
    uint8_t data_pin;           // データピン番号
    uint8_t clock_pin_base;     // クロックピンベース番号
    uint8_t pio_num;            // PIO インスタンス番号
    uint8_t dma_irq;            // DMA IRQ チャンネル
};
```

### サポートされる音声フォーマット
- `AUDIO_BUFFER_FORMAT_PCM_S16`: 16bit signed PCM
- `AUDIO_BUFFER_FORMAT_PCM_S32`: 32bit signed PCM

## 💡 使用例

### 基本的なサイン波生成
```c
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/audio.h"
#include <math.h>

#define SAMPLE_RATE 44100
#define FREQUENCY 440.0f  // A4音

int main() {
    stdio_init_all();
    
    // オーディオフォーマット設定
    audio_format_t audio_format = {
        .sample_freq = SAMPLE_RATE,
        .format = AUDIO_BUFFER_FORMAT_PCM_S32,
        .channel_count = 2
    };
    
    // I2S 初期化
    struct audio_i2s_config config = audio_i2s_default_config();
    audio_i2s_setup(&audio_format, &config);
    
    // バッファプール作成
    audio_buffer_pool_t *producer_pool = audio_new_producer_pool(&audio_format, 3, 512);
    
    // ストリーミング開始
    bool ok = audio_i2s_connect(producer_pool);
    assert(ok);
    audio_i2s_set_enabled(true);
    
    // サイン波生成ループ
    float phase = 0.0f;
    const float phase_increment = 2.0f * M_PI * FREQUENCY / SAMPLE_RATE;
    
    while (true) {
        audio_buffer_t *buffer = take_audio_buffer(producer_pool, true);
        int32_t *samples = (int32_t *) buffer->buffer->bytes;
        
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            int32_t value = (int32_t)(sinf(phase) * 0.5f * (1 << 30));
            samples[i * 2 + 0] = value;  // 左チャンネル
            samples[i * 2 + 1] = value;  // 右チャンネル
            
            phase += phase_increment;
            if (phase >= 2.0f * M_PI) {
                phase -= 2.0f * M_PI;
            }
        }
        
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(producer_pool, buffer);
    }
    
    return 0;
}
```

### ファイルからのオーディオ再生
```c
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/audio.h"
#include "ff.h"  // FatFs for SD card

void play_wav_file(const char *filename) {
    FIL file;
    FRESULT fr;
    
    // ファイルを開く
    fr = f_open(&file, filename, FA_READ);
    if (fr != FR_OK) {
        printf("Failed to open file: %s\n", filename);
        return;
    }
    
    // WAV ヘッダーをスキップ (簡略化)
    f_lseek(&file, 44);
    
    // オーディオフォーマット設定
    audio_format_t audio_format = {
        .sample_freq = 44100,
        .format = AUDIO_BUFFER_FORMAT_PCM_S16,
        .channel_count = 2
    };
    
    // I2S 初期化
    struct audio_i2s_config config = audio_i2s_default_config();
    audio_i2s_setup(&audio_format, &config);
    
    audio_buffer_pool_t *producer_pool = audio_new_producer_pool(&audio_format, 3, 1024);
    audio_i2s_connect(producer_pool);
    audio_i2s_set_enabled(true);
    
    // ファイル再生ループ
    while (!f_eof(&file)) {
        audio_buffer_t *buffer = take_audio_buffer(producer_pool, true);
        UINT bytes_read;
        
        fr = f_read(&file, buffer->buffer->bytes, 
                   buffer->max_sample_count * sizeof(int16_t) * 2, &bytes_read);
        
        if (fr != FR_OK || bytes_read == 0) break;
        
        buffer->sample_count = bytes_read / (sizeof(int16_t) * 2);
        give_audio_buffer(producer_pool, buffer);
    }
    
    f_close(&file);
}
```

### デュアルコア処理
```c
// audio_i2s.c をコンパイル時に CORE1_PROCESS_I2S_CALLBACK を定義

#include "pico/multicore.h"

// Core1 で実行される音声生成関数
void core1_audio_generation() {
    while (true) {
        // 音声データ生成処理
        // この処理は Core1 で実行され、Core0 のメイン処理に影響しない
        generate_complex_audio_data();
    }
}

int main() {
    stdio_init_all();
    
    // Core1 を起動
    multicore_launch_core1(core1_audio_generation);
    
    // I2S 初期化 (Core0)
    setup_i2s_audio();
    
    // メイン処理 (Core0)
    while (true) {
        // UI処理、制御処理など
        handle_user_interface();
        sleep_ms(10);
    }
}
```

## 🔧 高度な設定

### カスタムピン配置
```c
// カスタムピン設定を使用する場合
struct audio_i2s_config config = {
    .data_pin = 20,           // GP20 をデータピンに
    .clock_pin_base = 18,     // GP18/GP19 をクロックピンに
    .pio_num = 1,             // PIO1 を使用
    .dma_irq = 1              // DMA IRQ 1 を使用
};
```

### 動的サンプリング周波数変更
```c
// 再生中にサンプリング周波数を変更
audio_i2s_set_frequency(48000);  // 48 KHz に変更
sleep_ms(100);
audio_i2s_set_frequency(96000);  // 96 KHz に変更
```

## ⚠️ 注意事項

1. **PIO リソース**: このライブラリは指定された PIO インスタンスを占有します
2. **DMA チャンネル**: 指定された DMA IRQ チャンネルを使用します
3. **メモリ使用量**: バッファプールサイズに注意してください
4. **タイミング**: 高サンプリング周波数では CPU 負荷が高くなります
5. **ピン配置**: I2S ピンは連続している必要があります (BCK, LRCK)

## 🐛 デバッグのヒント

- **オーディオが出力されない**: ピン配置と DAC の配線を確認
- **ノイズが発生**: バッファアンダーランを避けるため、十分なバッファサイズを確保
- **周波数が不正確**: クロック設定を確認し、可能な分周比を計算
- **メモリ不足**: バッファプールサイズを調整