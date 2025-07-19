# 🎵 32bit I2S サイン波ジェネレーター

**Raspberry Pi Pico/Pico 2 用のインタラクティブ・デュアルチャンネル・サイン波ジェネレーター**

## 📖 概要

このサンプルは、pico_audio_i2s_32b ライブラリを使用して高品質な32bit ステレオサイン波を生成し、I2S DAC から出力するプログラムです。リアルタイムでの音量調整と左右独立した周波数制御が可能で、オーディオライブラリの機能を実演します。

## ✨ 主な機能

- 🎛️ **リアルタイム音量制御**: キーボード入力で即座に音量調整
- 🔊 **デュアルチャンネル独立制御**: 左右チャンネルの周波数を個別に設定
- 🎯 **32bit 高精度出力**: プロ品質のオーディオ出力
- ⚡ **DMA ベースの高効率処理**: CPU 負荷を最小限に抑制
- 🎵 **プリ計算テーブル**: 高速なサイン波生成

## 🎮 操作方法

| キー | 機能 | 説明 |
|------|------|------|
| `+` / `=` | 音量アップ | 音量レベルを増加 (0-256) |
| `-` | 音量ダウン | 音量レベルを減少 |
| `[` | 左チャンネル周波数ダウン | 左チャンネルの周波数を下げる |
| `]` | 左チャンネル周波数アップ | 左チャンネルの周波数を上げる |
| `{` | 右チャンネル周波数ダウン | 右チャンネルの周波数を下げる |
| `}` | 右チャンネル周波数アップ | 右チャンネルの周波数を上げる |
| `q` | 終了 | プログラムを終了 |

## 📌 ピン配置

### I2S DAC 接続
詳細な配線については、[メインドキュメント](../../README.md#📌-ピン配置) を参照してください。

### シリアル接続（デバッグ用）
| Pico Pin | GPIO | 機能 | CP2102 モジュール |
|----------|------|------|-------------------|
| 1 | GP0 | UART0_TX | RXD |
| 2 | GP1 | UART0_RX | TXD |
| 3 | GND | GND | GND |

## 🔧 ハードウェア要件

### 必要なデバイス
- Raspberry Pi Pico または Pico 2
- PCM5102 DAC ボード（推奨）または ES9023 DAC ボード
- スピーカーまたはヘッドフォン
- USB シリアル変換器（CP2102 等、操作用）

## 🚀 ビルドと実行

### クイックスタート
```bash
# VS Code でプロジェクトを開く
cd ~/pico-development/pico_audio_i2s_32b
code .

# タスクを実行
# Ctrl+Shift+P → Tasks: Run Task → Build Sample (sine_wave_i2s_32b)
```

### コマンドライン
```bash
cd samples/sine_wave_i2s_32b
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

### アップロード
1. BOOTSEL ボタンを押しながら Pico を USB 接続
2. `sine_wave_i2s_32b.uf2` を RPI-RP2 ドライブにコピー

## 💡 技術的詳細

### オーディオ仕様
- **サンプリング周波数**: 44.1 kHz (CD品質)
- **ビット深度**: 32bit signed PCM
- **チャンネル数**: 2ch (ステレオ)
- **バッファサイズ**: 1156 サンプル/チャンネル

### アルゴリズムの特徴

#### 高速サイン波生成
```cpp
// プリ計算テーブルを使用した高速サイン波生成
static int16_t sine_wave_table[SINE_WAVE_TABLE_LEN];

// 初期化時にテーブルを生成
for (int i = 0; i < SINE_WAVE_TABLE_LEN; i++) {
    sine_wave_table[i] = 32767 * cosf(i * 2 * M_PI / SINE_WAVE_TABLE_LEN);
}

// 実行時は単純なテーブル参照
int32_t value = (vol * sine_wave_table[pos >> 16u]) << 8u;
```

#### 固定小数点演算
```cpp
// 周波数制御用の固定小数点位相管理
uint32_t step = frequency_multiplier;  // 16.16 固定小数点
uint32_t pos = phase_accumulator;      // 位相アキュムレータ

pos += step;  // 位相を進める
if (pos >= pos_max) pos -= pos_max;  // ラップアラウンド
```

#### 32bit フルスケール変換
```cpp
// 16bit テーブル値を 32bit フルスケールに変換
// ディザリング効果も含む
samples[i*2+0] = value0 + (value0 >> 16u);  // 左チャンネル
samples[i*2+1] = value1 + (value1 >> 16u);  // 右チャンネル
```

### システム最適化

#### クロック設定
- システムクロック: 96MHz (高精度オーディオ用)
- DCDC PSM制御: PWM モード (ノイズ低減)

#### DMA 割り込み処理
```cpp
// DMA IRQ ハンドラからのコールバック
void i2s_callback_func() {
    if (decode_flg) {
        decode();  // 非ブロッキングでバッファを更新
    }
}
```

## 📊 パフォーマンス

### CPU 使用率
- **メインループ**: ~1% (キーボード入力処理のみ)
- **DMA コールバック**: ~5% (サイン波生成)
- **合計**: ~6% (Core0 で実行)

### メモリ使用量
- **サイン波テーブル**: 4KB (2048 × 16bit)
- **オーディオバッファ**: ~14KB (3バッファ × 1156サンプル × 8バイト)
- **スタック使用量**: ~2KB

## 🔧 カスタマイズ

### 周波数範囲の変更
```cpp
// samples/sine_wave_i2s_32b/sine_wave.cpp の step 制限を変更
if (c == ']' && step0 < (SINE_WAVE_TABLE_LEN / 8) * 0x20000) step0 += 0x10000;
//                     ↑ この値を変更して最大周波数を調整
```

### サンプリング周波数の変更
```cpp
// main() 関数内
ap = i2s_audio_init(48000);  // 48kHz に変更
```

### バッファサイズの調整
```cpp
// レイテンシを下げたい場合
#define SAMPLES_PER_BUFFER 512  // デフォルト: 1156
```

## 🐛 トラブルシューティング

### よくある問題

#### 音が出ない
1. DAC ボードの電源供給を確認
2. ピン配置を確認 (GP16: BCK, GP17: LRCK, GP18: SDO)
3. PCM5102 のジャンパー設定を確認

#### ノイズが発生する
1. グランド接続を確認
2. 電源ノイズを除去
3. DCDC 設定が PWM モードになっているか確認

#### 周波数が期待値と異なる
1. サイン波テーブル長が適切か確認
2. step 値の計算を確認
3. サンプリング周波数設定を確認

## 📚 参考資料

- [API リファレンス](../../docs/API.md)
- [ビルド手順書](../../docs/BUILD.md)
- [PCM5102 データシート](https://www.ti.com/lit/ds/symlink/pcm5102.pdf)
- [Raspberry Pi Pico ドキュメント](https://datasheets.raspberrypi.org/pico/pico-datasheet.pdf)