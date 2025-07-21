# CLAUDE.md
全て日本語で回答して下さい
This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## コミット設定
- コミットメッセージにClaude Code署名や共同著者行を含めない
- シンプルなコミットメッセージのみ使用

## GitHub Actions CI/CD Pipeline
- 完全なCI/CDパイプラインが実装済み
- コミット後は必ずGitHub Actionsのログを確認してエラー対応を実行

### コミット後の必須確認手順
**注意**: ソースコード・設定ファイル・ワークフローを変更した場合のみ実行。ドキュメントのみの変更時は不要。

```bash
# 1. プッシュ後の確認（ソースコード変更時のみ）
git push origin main
sleep 30
gh run list --limit 3

# 2. エラー時の詳細分析
gh run view [run-id] --log-failed

# 3. 実行中ワークフローの監視
gh run view [run-id]
```

### 重要な修正ノウハウ
- **cppcheck**: 埋め込み開発では `--suppress=missingInclude,unknownMacro,cstyleCast` が必要
- **clang-format**: 厳密チェックは警告のみに変更（`--Werror` を削除）
- **タブ文字チェック**: `libs/` ディレクトリは除外（pico-extras含むため）
- **VS Code settings.json**: JSONコメント削除が必要（標準JSONツールとの互換性）
- **ハードコードパス**: ドキュメント（*.md）は除外して実行可能

### GitHub CLI認証設定
```bash
gh auth login --web
gh repo set-default tsugumasa320/pico_audio_i2s_32b
```

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

#### CMAKE_ASM_COMPILE_OBJECTエラーの解決
**症状:**
```
CMake Error: Error required internal CMake variable not set, cmake may not be built correctly.
Missing variable is: CMAKE_ASM_COMPILE_OBJECT
```

**✅ 解決策: プロジェクト言語の明示的指定**
```cmake
# CMakeLists.txtで言語を明示的に指定
project(project_name C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
```

**環境変数での対処:**
```bash
# ツールチェインパスを明示的に設定
PICO_SDK_PATH=/path/to/pico-sdk PICO_TOOLCHAIN_PATH=/Users/username/.pico-sdk/toolchain/14_2_Rel1 cmake ...
```

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

### アナログマルチプレクサー (74HC4051) 実装

8個のアナログ入力（ノブ）を1つのADCで読み取るマルチプレクサー機能を実装済み。

**ファイル構成:**
- `products/cross_fm_noise_synth/include/analog_mux.h` - マルチプレクサー制御クラス
- `samples/sine_wave_i2s_32b/analog_mux.h` - サンプル実装

**基本的な使用方法:**
```cpp
AnalogMux g_analog_mux;
AnalogMux::Config mux_config = {
    .pin_enable = 0, .pin_s0 = 3, .pin_s1 = 2, .pin_s2 = 1,
    .adc_pin = 26, .adc_channel = 0, .scan_period_ms = 10,
    .enable_active_low = true
};
g_analog_mux.Init(mux_config);

// 値取得
g_analog_mux.Update();
float volume = g_analog_mux.GetNormalizedValue(0);  // 0.0-1.0
```

📖 **詳細は専用ドキュメントを参照:** [docs/ANALOG_MUX.md](docs/ANALOG_MUX.md)

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

### ローカルビルド手順（重要）

#### **必須**: コミット前の確認作業
```bash
# 1. ローカルでビルドテストを必ず実行
cd samples/sine_wave_i2s_32b && rm -rf build && mkdir build
PICO_SDK_PATH=/path/to/pico-sdk cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 -S . -B build
make -C build

# 2. 製品プロジェクトのビルドテスト
cd products/cross_fm_noise_synth && rm -rf build && mkdir build
PICO_SDK_PATH=/path/to/pico-sdk cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 -S . -B build
make -C build

# 3. ビルド成功後にコミット・プッシュ
git add .
git commit -m "message"
git push origin main
```

#### ビルド時の注意点
- **PICO_SDK_PATH**: 環境変数で正しいSDKパスを指定
- **CMAKE_ASM_COMPILER**: ASMコンパイラーエラーが出る場合は適切に設定
- **libs/pico-extras**: プロジェクト内に含まれているパスを正しく指定
- **DaisySP**: サブモジュールとして追加済み、初期化必要な場合は `git submodule update --init`

#### VS Codeタスク利用
- `Build Product (Cross FM Noise Synth)`: 製品ビルド
- `Build Sample (sine_wave_i2s_32b)`: サンプルビルド  
- `Clean Build`: 全ビルドディレクトリクリア

### I2Sオーディオ出力の重要ノウハウ

#### ❌ よくある失敗パターンと ✅ 正しい実装

**1. バッファサイズ設定**
- ❌ 不適切: 任意のサイズ（256など）
- ✅ 正解: 動作実績のあるサイズ（`SAMPLES_PER_BUFFER = 1156`）
- 理由: I2SとDMAの同期に適したサイズが重要

**2. DACゼロレベル設定**
- ❌ 不適切: `DAC_ZERO = 0`  
- ✅ 正解: `DAC_ZERO = 1`
- 理由: I2S DACの特性に合わせた適切なオフセット値

**3. システムクロック設定順序**
```cpp
// ❌ 不適切
stdio_init_all();
pll_init(pll_usb, ...);
clock_configure(clk_sys, ...);
// clk_peri設定なし、stdio再初期化なし

// ✅ 正解  
stdio_init_all();
sleep_ms(2000);  // USBシリアル安定化
pll_init(pll_usb, ...);
clock_configure(clk_usb, ...);
clock_configure(clk_sys, ...);
clock_configure(clk_peri, ...);  // 周辺機器クロック必須
stdio_init_all();  // クロック変更後の再初期化必須
```

**4. DCDC設定（必須）**
```cpp
// オーディオノイズ低減のため必須
const uint32_t PIN_DCDC_PSM_CTRL = 23;
gpio_init(PIN_DCDC_PSM_CTRL);
gpio_set_dir(PIN_DCDC_PSM_CTRL, GPIO_OUT);
gpio_put(PIN_DCDC_PSM_CTRL, 1); // PWMモード
```

**5. 初期化シーケンスの厳守**
```cpp
// ✅ 正解の順序（sine_wave_i2s_32bで実証済み）
// 1. stdio_init_all()
// 2. sleep_ms(2000)
// 3. システムクロック設定
// 4. 周辺機器クロック設定  
// 5. stdio_init_all() 再実行
// 6. DCDC PWMモード設定
// 7. オーディオバッファプール作成
// 8. I2Sセットアップ
// 9. バッファプール接続
// 10. 初期バッファ設定（DAC_ZERO値）
// 11. I2S有効化
```

#### 新規オーディオプロジェクト作成時の必須チェックリスト

1. **動作実績コードからコピー**: sine_wave_i2s_32bから初期化部分をコピー
2. **バッファサイズ**: `SAMPLES_PER_BUFFER = 1156` を使用
3. **DAC設定**: `DAC_ZERO = 1` を使用
4. **クロック設定**: clk_peri設定とstdio再初期化を含める
5. **DCDC設定**: PIN_DCDC_PSM_CTRL = 23でPWMモード
6. **初期化順序**: 上記シーケンスを厳守
7. **デバッグ**: 各ステップでprintf出力を追加

#### トラブルシューティング手順

**音が出ない場合:**
1. 動作するsine_wave_i2s_32bサンプルでテスト
2. 新規プロジェクトの初期化を上記チェックリストと比較
3. バッファサイズとDAC_ZERO値を確認
4. システムクロックとclk_peri設定を確認
5. 初期化シーケンスの順序を確認

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

## Cross FM Noise Synthesizer 開発ノウハウ

### プロジェクト概要
products/cross_fm_noise_synth/ - DaisySPとPico SDKを統合した2オペレーターFM相互変調シンセサイザー

### 開発プロセスと重要な学び

#### 1. 音声出力のトラブルシューティング手法

**段階的デバッグアプローチ**:
```cpp
// 1. シンプルなサイン波から開始
#define DEBUG_FALLBACK_SINE 1   // フォールバック用サイン波
// 2. 単一FMシンセ
// 3. デュアルFMシンセ + クロスモジュレーション
// 4. エフェクトチェーン追加
```

**音が出ない問題の切り分け**:
- LEDデバッグで実行状況を視覚確認
- バッファ処理の範囲確認（1156サンプル中2サンプルのみ処理など）
- DaisySP初期化の問題切り分け

#### 2. DaisySP統合のベストプラクティス

**正しい初期化順序**:
```cpp
const float sample_rate = 48000.0f;  // 参照版と一致させる
fm1.Init(sample_rate);
fm1.SetFrequency(440.0f);
fm1.SetRatio(0.5f); 
fm1.SetIndex(100.0f);
```

**避けるべき問題**:
- サンプルレート不一致（44.1kHz vs 48kHz）
- 初期化前のProcess()呼び出し
- パラメータ範囲外設定

#### 3. アナログマルチプレクサー（74HC4051）実装

**高速スキャン設定**:
```cpp
AnalogMux::Config mux_config = {
    .scan_period_ms = 1,  // 1ms高速スキャン（参照版準拠）
    .enable_active_low = true
};
```

**ピンアサイン（参照版と完全一致）**:
- GP0: /EN (Enable, active low)
- GP1: S2, GP2: S1, GP3: S0 (Select bits)
- GP26: COM (ADC input)

#### 4. 意図的破綻設計の実装

**参照版の「美しいバグ」の再現**:
```cpp
// val0=0で最高音質になる逆転ロジック
if (val0 > 0) { // ここは0が一番音が良い気がする
    out1 = fm1.Process();
} else {
    out1 = 0.0f;
}

// 直接乗算による意図的クロスモジュレーション（破綻の原因）
if (i % 2 == 0) {
    fm1.SetFrequency(scaleValue(val0, 0, 1023, 0.0f, 1000.0f) * out2);
    fm2.SetFrequency(scaleValue(val3, 0, 1023, 0.0f, 1000.0f) * out1);
}
```

#### 5. Arduino→Pico SDK変換のコツ

**バッファ処理の変換**:
```cpp
// Arduino: BUFFER_SIZE = 2 サンプルずつ処理
// Pico SDK: sample_countサンプル一括処理
for (uint32_t i = 0; i < sample_count; i++) {
    // 処理内容
}
```

**環境固有の関数置換**:
- `random()` → `rand()`
- `delay()` → `sleep_ms()`
- `millis()` → `to_ms_since_boot(get_absolute_time())`

#### 6. デバッグテクニック

**LEDによる視覚デバッグ**:
```cpp
const uint LED_PIN = 25;  // Pico 2内蔵LED
// 起動確認: 3回点滅
// 初期化完了: 常時点灯  
// 音声処理中: 1秒ごと点滅
```

**シリアル出力が使えない場合**:
- BOOTSELモード転送確認
- LEDパターンによる状態把握
- ビルド時間スタンプでの転送確認

#### 7. パフォーマンス最適化

**設定可能バッファサイズ**:
```cpp
#ifndef SAMPLES_PER_BUFFER
#define SAMPLES_PER_BUFFER 64   // 低レイテンシー
// #define SAMPLES_PER_BUFFER 128  // バランス
// #define SAMPLES_PER_BUFFER 256  // 標準
// #define SAMPLES_PER_BUFFER 1156 // 高安定性
#endif
```

**リアルタイム処理の注意点**:
- Core1での音声処理分離
- バッファあたりの処理時間制限
- パラメータ更新頻度の調整

#### 8. 参照実装との互換性維持

**重要な設定値**:
- サンプルレート: 48000Hz（44100Hzではない）
- DAC_ZERO: 1（0ではない）
- スキャン周期: 1ms（10msではない）
- ボリュームスケーリング: -70dB〜+6dB

**音色の再現**:
- FM1: 440Hz, ratio=0.5, index=100
- FM2: 330Hz, ratio=0.33, index=50
- クロスモジュレーション周期: 2サンプルごと

#### 9. よくある問題と解決法

**問題**: 音量が小さすぎる
**解決**: dBスケーリングとアナログ音量の併用

**問題**: 音が断続的
**解決**: バッファアンダーランの確認、処理時間測定

**問題**: ノイズや歪み
**解決**: DCDCのPWMモード設定、グラウンドループ対策

#### 10. 今後の拡張可能性

**追加可能なエフェクト**:
- DCブロックフィルター
- アンチエイリアシングフィルター
- 3バンドEQ
- リバーブ・ディレイ

**インターフェース拡張**:
- MIDI入力対応
- 複数プリセット保存
- ウェブベースコントロール