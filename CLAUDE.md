# CLAUDE.md
全て日本語で回答して下さい
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## GitHub Actions CI/CD Pipeline
- 完全なCI/CDパイプラインが実装済み
- コミット後は必ずGitHub Actionsのログを確認してエラー対応を実行

## Project Overview

This is a 32-bit I2S DAC library for Raspberry Pi Pico/Pico 2, supporting stereo audio output up to 192 KHz sampling frequency. The library uses PIO (Programmable I/O) to implement I2S audio interface.

## Build Commands

### Prerequisites
- Set environment variable: `PICO_SDK_PATH` (pointing to external pico-sdk)
- Confirmed with pico-sdk 2.1.1
- Note: pico-extras is included in libs/pico-extras/ within the repository
- pico-examples is included in pico-examples/ within the repository

### Windows (Developer Command Prompt for VS 2022)
```bash
cd samples/xxxxx  # sample project directory
mkdir build && cd build
cmake -G "NMake Makefiles" ..                                                      # Pico 1
cmake -G "NMake Makefiles" -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..          # Pico 2
nmake
```

### Linux
```bash
cd samples/xxxxx  # sample project directory
mkdir build && cd build
cmake ..                                                    # Pico 1
cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..        # Pico 2
make -j4
```

## Code Architecture

### Core Components

#### Main Library (`pico_audio_i2s/`)
- `audio_i2s.c` - Main I2S implementation using PIO and DMA
- `audio_i2s.pio` - PIO assembly program for I2S timing
- `include/pico/audio_i2s.h` - Public API definitions

#### Audio Processing (`pico_audio_i2s/pico_audio_32b/`)
- `audio.cpp` - Audio buffer management and format conversion
- `audio_utils.S` - Assembly utilities for audio processing
- `include/pico/audio.h` - Audio framework definitions
- `include/pico/sample_conversion.h` - Sample format conversion utilities

#### Configuration Macros (audio_i2s.h)
- `PICO_AUDIO_I2S_DATA_PIN` (default: 18) - I2S data pin
- `PICO_AUDIO_I2S_CLOCK_PIN_BASE` (default: 16) - I2S clock pins base
- `PICO_AUDIO_I2S_PIO` (default: 0) - PIO instance to use
- `PICO_AUDIO_I2S_DMA_IRQ` (default: 0) - DMA IRQ channel
- `CORE1_PROCESS_I2S_CALLBACK` - Enable dual-core processing

### Key Features
- Supports 16-bit and 32-bit PCM formats
- Stereo audio (mono-to-stereo conversion available)
- Dynamic frequency adjustment
- Double-buffered DMA for continuous playback
- Optional dual-core processing with Core1 handling callbacks

### Sample Projects
Located in `samples/` directory with individual CMakeLists.txt and README files showing usage patterns.

## Hardware Support
- Raspberry Pi Pico/Pico 2
- PCM5102 32-bit I2S DAC
- ES9023 24-bit I2S DAC

## Pin Mapping (Default)
- GP16: BCK (bit clock)
- GP17: LRCK (left/right clock)
- GP18: SDO (serial data output)
- VBUS: 5V power for DAC

## 開発ノウハウと重要事項

### VS Code ビルド設定

#### ❌ 問題: RP2040/RP2350プラットフォーム切り替え失敗
VS Codeでプラットフォームを切り替える際、CMakeキャッシュが原因で古い設定が残る問題が発生する。

**症状:**
```
Family ID 'rp2040' cannot be downloaded anywhere
ERROR: This file cannot be loaded onto a device with no partition table
```

#### ✅ 解決策: buildディレクトリの完全削除
```bash
# VS Code tasks.json で実装済み
rm -rf build && mkdir build && 
PICO_SDK_PATH=... PICO_EXTRAS_PATH=... cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 .
```

**重要:** VS Codeタスクの環境変数は明示的に設定する必要がある。taskの依存関係では環境変数が継承されない。

### 音声品質の調整

#### 音量設定による歪み制御
- **推奨値**: `uint vol = 8` (30%レベル)
- **避けるべき値**: `vol = 20` 以上（クリッピング歪みが発生）
- **32bit PCMでの計算**: `value = (vol * sine_table[pos]) << 8`

#### サンプリング周波数の動的変更
```c
// 動的な周波数変更をサポート
update_pio_frequency(new_freq, pcm_format, channel_count);
```

### CMakeキャッシュ管理

#### プラットフォーム変更時の必須作業
1. **キャッシュ削除**: `rm -rf build`
2. **環境変数設定**: 明示的に`PICO_PLATFORM`と`PICO_BOARD`を指定
3. **確認**: CMakeCache.txtで設定が正しく反映されているかチェック

#### 環境変数の優先順位
```bash
# 正しい設定例
export PICO_SDK_PATH="/path/to/pico-sdk"  # 外部SDKへのパス
# PICO_EXTRAS_PATHとPICO_EXAMPLES_PATHはプロジェクト内に含まれているため設定不要
```

### デバッグとトラブルシューティング

#### オーディオ出力の診断
1. **USBシリアル確認**: デバイスが認識されているか
2. **UF2ファイル確認**: `file *.uf2`でプラットフォーム確認
3. **クロック設定**: 96MHz動作の確認
4. **DCDC設定**: PWMモードでノイズ低減

#### ビルドエラーの対処
- **依存関係エラー**: pico-extrasのパスを確認
- **PIOコンパイルエラー**: pioasmのビルドとインストール確認
- **DMAエラー**: チャンネル競合の確認

### パフォーマンス最適化

#### メモリ使用量
```c
// バッファサイズの計算
// buffers × channels × sample_size × buffer_length
// 例: 3 × 2 × 4 × 576 = 13.8KB
#define PICO_AUDIO_I2S_BUFFER_SAMPLE_LENGTH 576u
```

#### リアルタイム処理
- **i2s_callback_func()**: 割り込みコンテキストでの実行
- **処理時間制限**: 1バッファ分の時間内で完了必須
- **Core1処理**: `CORE1_PROCESS_I2S_CALLBACK`での分離可能

### コードメンテナンス

#### ドキュメント化の方針
- **API関数**: Doxygen形式のコメント必須
- **内部実装**: 動作原理の詳細説明
- **設定マクロ**: 使用例と推奨値の記載
- **エラー処理**: assert()での前提条件チェック

#### サンプルコードの活用
```cpp
// samples/sine_wave_i2s_32b/ 内のサンプルファイル
sine_wave.cpp           // メインのサイン波生成サンプル
```

### 今後の開発指針

#### 機能拡張の検討事項
- モノラル音声の完全サポート
- 8bit PCMフォーマットの対応
- 可変ビットレート対応
- 複数I2S出力の同期

#### 保守性の向上
- エラーハンドリングの強化
- メモリリーク防止の徹底
- プラットフォーム依存部分の分離
- 自動テストの充実