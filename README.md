# 🎵 pico_audio_i2s_32b

**高性能 32bit I2S DAC ライブラリ for Raspberry Pi Pico / Pico 2**

[![Build Status](https://github.com/elehobica/pico_audio_i2s_32b/actions/workflows/build-binaries.yml/badge.svg)](https://github.com/elehobica/pico_audio_i2s_32b/actions/workflows/build-binaries.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform](https://img.shields.io/badge/platform-Raspberry%20Pi%20Pico-green.svg)](https://www.raspberrypi.org/products/raspberry-pi-pico/)

## 🌟 概要

pico_audio_i2s_32b は、Raspberry Pi Pico および Pico 2 向けの高性能 32bit ステレオ I2S DAC ライブラリです。PIO (Programmable I/O) を活用した正確なタイミング制御と DMA による効率的なデータ転送により、プロ品質のオーディオ出力を実現します。

### ✨ 主な特徴

- 🎯 **32bit ステレオ音声出力** - CD 品質を超える高解像度オーディオ
- 🚀 **最大 192 KHz サンプリング** - ハイレゾ音源に対応
- ⚡ **PIO ベースの高精度タイミング** - ジッターの少ない安定した出力
- 🔄 **DMA による効率的転送** - CPU 負荷を最小限に抑制
- 🧠 **デュアルコア対応** - Core1 での音声処理によるパフォーマンス向上
- 🎛️ **動的周波数変更** - 再生中のサンプリング周波数切り替え
- 🔧 **柔軟なピン配置** - カスタマイズ可能な GPIO 設定

## 🛠️ サポートデバイス

### マイコンボード
- 🔴 **Raspberry Pi Pico** (RP2040)
- 🟢 **Raspberry Pi Pico 2** (RP2350)

### 対応 DAC チップ
- 🎵 **PCM5102** - 32bit I2S Audio DAC (推奨)
- 🎵 **ES9023** - 24bit I2S Audio DAC

### オーディオ仕様
| 項目 | 仕様 |
|------|------|
| **チャンネル数** | 2ch (ステレオ) |
| **ビット深度** | 16bit / 32bit |
| **サンプリング周波数** | 8 KHz ~ 192 KHz |
| **出力インピーダンス** | DAC 依存 |
| **S/N 比** | DAC 依存 (PCM5102: 112 dB) |

## 📌 ピン配置

### デフォルト配置

| Pico Pin | GPIO | Function | 接続先 | 説明 |
|----------|------|----------|--------|------|
| 21 | GP16 | BCK | PCM5102 BCK (13) | ビットクロック |
| 22 | GP17 | LRCK | PCM5102 LRCK (15) | 左右チャンネルクロック |
| 23 | GND | GND | GND | グランド |
| 24 | GP18 | SDO | PCM5102 DIN (14) | シリアルデータ出力 |
| 40 | VBUS | VCC | DAC VIN | 5V 電源供給 |

### 回路図

![PCM5102 接続図](doc/PCM5102_Schematic.png)

### PCM5102 ボード設定

PCM5102 DAC ボードのジャンパー設定：

| ジャンパー | 設定 | 説明 |
|------------|------|------|
| **SCK** | LOW (ショート) | システムクロックを無効化 |
| **H1L (FLT)** | LOW | デジタルフィルター有効 |
| **H2L (DEMP)** | LOW | デエンファシス無効 |
| **H3L (XSMT)** | HIGH | ソフトミュート無効 |
| **H4L (FMT)** | LOW | I2S フォーマット選択 |

### カスタムピン配置

ピン配置は設定で変更可能です：

```c
struct audio_i2s_config config = {
    .data_pin = 20,           // SDO ピン
    .clock_pin_base = 18,     // BCK ピン (LRCK は +1)
    .pio_num = 1,             // PIO インスタンス
    .dma_irq = 1              // DMA IRQ チャンネル
};
```

## 🚀 クイックスタート

### 1. 開発環境のセットアップ

詳細なセットアップ手順については、[ビルド手順書](docs/BUILD.md) を参照してください。

```bash
# 依存関係のインストール (macOS)
brew install arm-none-eabi-gcc cmake git

# Pico SDK のセットアップ
mkdir ~/pico-development && cd ~/pico-development
git clone -b 2.1.1 https://github.com/raspberrypi/pico-sdk.git
git clone -b sdk-2.1.1 https://github.com/raspberrypi/pico-extras.git
git clone https://github.com/tsugumasa320/pico_audio_i2s_32b.git

# 環境変数の設定
export PICO_SDK_PATH=~/pico-development/pico-sdk
export PICO_EXTRAS_PATH=~/pico-development/pico-extras
```

### 2. サンプルプロジェクトのビルド

#### VS Code を使用（推奨）
```bash
cd ~/pico-development/pico_audio_i2s_32b
code .
```
- `Ctrl+Shift+P` → `Tasks: Run Task` → `Build Sample 　(sine_wave_i2s_32b)` or `Upload to Pico (picotool)` 

#### コマンドライン
```bash
cd ~/pico-development/pico_audio_i2s_32b/samples/sine_wave_i2s_32b
mkdir build && cd build

# Pico 1 用
cmake -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
      -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
      -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc ..

# Pico 2 用
cmake -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
      -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
      -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc \
      -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..

make -j4
```

### 3. Pico へのアップロード

1. **BOOTSEL** ボタンを押しながら Pico を USB 接続
2. 生成された `.uf2` ファイルを `RPI-RP2` ドライブにコピー

## 💻 基本的な使用方法

### 最小限のサイン波生成

```c
#include "pico/stdlib.h"
#include "pico/audio_i2s.h"
#include "pico/audio.h"
#include <math.h>

int main() {
    stdio_init_all();
    
    // オーディオフォーマット設定
    audio_format_t audio_format = {
        .sample_freq = 44100,
        .format = AUDIO_BUFFER_FORMAT_PCM_S32,
        .channel_count = 2
    };
    
    // I2S 初期化
    struct audio_i2s_config config = audio_i2s_default_config();
    audio_i2s_setup(&audio_format, &config);
    
    // バッファプール作成
    audio_buffer_pool_t *producer_pool = audio_new_producer_pool(&audio_format, 3, 512);
    audio_i2s_connect(producer_pool);
    audio_i2s_set_enabled(true);
    
    // サイン波生成
    float phase = 0.0f;
    while (true) {
        audio_buffer_t *buffer = take_audio_buffer(producer_pool, true);
        int32_t *samples = (int32_t *) buffer->buffer->bytes;
        
        for (uint i = 0; i < buffer->max_sample_count; i++) {
            int32_t value = (int32_t)(sinf(phase) * 0.3f * (1 << 30));
            samples[i * 2 + 0] = value;  // 左チャンネル
            samples[i * 2 + 1] = value;  // 右チャンネル
            phase += 2.0f * M_PI * 440.0f / 44100.0f;  // 440Hz
        }
        
        buffer->sample_count = buffer->max_sample_count;
        give_audio_buffer(producer_pool, buffer);
    }
}
```

## 📚 ドキュメント

- 📖 **[API リファレンス](docs/API.md)** - 詳細な API ドキュメント
- 🔧 **[ビルド手順書](docs/BUILD.md)** - 環境構築とビルド方法
- 💡 **[サンプルコード](samples/)** - 使用例とチュートリアル

## 🎯 応用例

### 対応済みプロジェクト
- 🎵 **[RPi_Pico_WAV_Player](https://github.com/elehobica/RPi_Pico_WAV_Player)** - WAV ファイル再生プレーヤー

### 実装可能なアプリケーション
- 🎤 **リアルタイム音声エフェクター**
- 🎹 **MIDI シンセサイザー**
- 📻 **インターネットラジオプレーヤー**
- 🔊 **マルチチャンネルオーディオミキサー**
- 🎵 **楽器チューナー**

## ⚙️ 設定オプション

### マクロ定義

```c
// ピン設定のカスタマイズ
#define PICO_AUDIO_I2S_DATA_PIN 18           // データピン
#define PICO_AUDIO_I2S_CLOCK_PIN_BASE 16     // クロックピンベース
#define PICO_AUDIO_I2S_PIO 0                 // PIO インスタンス
#define PICO_AUDIO_I2S_DMA_IRQ 0             // DMA IRQ チャンネル

// デュアルコア処理を有効化
#define CORE1_PROCESS_I2S_CALLBACK
```

### 対応サンプリング周波数

| 周波数 | 用途 | 品質 |
|--------|------|------|
| 8 KHz | 音声通話 | 電話品質 |
| 22.05 KHz | 低品質音楽 | AM ラジオ相当 |
| 44.1 KHz | CD 音質 | 標準音楽品質 |
| 48 KHz | DAT/DVD | 業務用標準 |
| 96 KHz | ハイレゾ | 高品質 |
| 192 KHz | 超ハイレゾ | 最高品質 |

## 🔧 トラブルシューティング

### よくある問題

#### 音が出ない
- ピン配置の確認
- DAC ボードの電源供給
- ジャンパー設定の確認

#### ノイズが発生
- グランドの接続確認
- 電源ノイズの除去
- サンプリング周波数の調整

#### ビルドエラー
- ARM GCC の インストール確認
- 環境変数の設定確認
- 依存関係の確認

詳細は [ビルド手順書のトラブルシューティング](docs/BUILD.md#トラブルシューティング) を参照。

## 📄 ライセンス

このプロジェクトは [MIT License](LICENSE) の下で公開されています。

## 🤝 コントリビューション

プルリクエストやイシューの報告を歓迎します！

1. このリポジトリをフォーク
2. フィーチャーブランチを作成 (`git checkout -b feature/amazing-feature`)
3. 変更をコミット (`git commit -m 'Add amazing feature'`)
4. ブランチにプッシュ (`git push origin feature/amazing-feature`)
5. プルリクエストを開く

## 🙏 謝辞

- [Raspberry Pi Foundation](https://www.raspberrypi.org/) - Pico SDK の開発
- [elehobica](https://github.com/elehobica) - オリジナルライブラリの作成
