# Cross FM Noise Synthesizer - 開発ノウハウ

このドキュメントは、Cross FM Noise Synthesizer の開発に関する詳細なノウハウを記載しています。

## 概要

本プロジェクトは、Arduino/PlatformIO環境の参照実装（`pico2_i2s_pio`）を Raspberry Pi Pico SDK 環境に移植したFMクロスモジュレーション・ノイズシンセサイザーです。

### 参照実装の場所
```
/Users/tsugumasayutani/Documents/GitHub/pico2_i2s_pio/src/main.cpp
```

## 意図的な「破綻」設計について

⚠️ **重要**: このシンセサイザーは意図的に音が「破綻」する設計です。これは仕様であり、バグではありません。

### 特徴的な実装パターン

#### 1. val0=0で最高音質
```cpp
if (val0 > 0){ // ここは0が一番音が良い気がする
    out1 = fm1.Process();
} else {
    out1 = 0.0f;
}
```

#### 2. 直接乗算による破綻的クロスモジュレーション
```cpp
// 意図的な破綻設計（直接乗算によるクロスモジュレーション）
if (i % 2 == 0) {
    fm1.SetFrequency(scaleValue(val0, 0, 1023, 0.0f, 1000.0f) * out2);
    fm1.SetIndex(scaleValue(val1, 0, 1023, 0.0f, 20.0f) * out2);
    fm1.SetRatio(scaleValue(val2, 0, 1023, 0.0f, 20.0f) * out2);
    fm2.SetFrequency(scaleValue(val3, 0, 1023, 0.0f, 1000.0f) * out1);
    fm2.SetIndex(scaleValue(val4, 0, 1023, 0.0f, 20.0f) * out1);
    fm2.SetRatio(scaleValue(val5, 0, 1023, 0.0f, 20.0f) * out1);
    overdrive.SetDrive(scaleValue(val6, 0, 1023, 0.0f, 1.0f));
}
```

## Arduino → Pico SDK 移植のポイント

### 1. ループ構造の変換

**Arduino版（参照）:**
```cpp
void loop() {
    if (i2s.availableForWrite() > BUFFER_SIZE) {
        for (int s = 0; s < BUFFER_SIZE; s++) {
            // 2サンプルのみ処理
        }
    }
}
```

**Pico SDK版:**
```cpp
void core1_audio_loop() {
    while (true) {
        audio_buffer_t *buffer = take_audio_buffer(g_audio_pool, true);
        for (uint32_t i = 0; i < sample_count; i++) {
            // 全サンプル処理（通常1156サンプル）
        }
    }
}
```

### 2. マルチプレクサー実装の違い

**Arduino版:**
```cpp
kinoshita_lab::AnalogMultiplexer74HC4051 multiplexer1(kPinNEnable, kPinS0, kPinS1, kPinS2, kAnalogIn);
const int val0 = multiplexer1.getFilteredValue(0);
```

**Pico SDK版:**
```cpp
AnalogMux g_analog_mux;
const int val0 = (int)(g_analog_mux.GetNormalizedValue(0) * 1023);
```

### 3. I2S出力の違い

**Arduino版:**
```cpp
sample = convert<BITPER_SAMPLE>(mixed_out);
writeStereo(i2s, sample, sample);
```

**Pico SDK版:**
```cpp
sample = (int32_t)(mixed_out * 2147483647.0f);
samples[i * 2 + 0] = sample;  // Left
samples[i * 2 + 1] = sample;  // Right
```

## ハードウェア設定

### アナログマルチプレクサー (74HC4051)
```cpp
enum {
    kPinNEnable = 0,  // Enable pin (active low)
    kPinS0      = 3,  // Select pin S0
    kPinS1      = 2,  // Select pin S1
    kPinS2      = 1,  // Select pin S2
    kAnalogIn   = 26, // ADC input pin
};
```

### ノブアサイン
- val0: FM1 Frequency Base (0-1000Hz) - **0 = BEST SOUND!**
- val1: FM1 Index Base (0-20)
- val2: FM1 Ratio Base (0-20)
- val3: FM2 Frequency Base (0-1000Hz)
- val4: FM2 Index Base (0-20)
- val5: FM2 Ratio Base (0-20)
- val6: Overdrive Drive (0.0-1.0)
- val7: Master Volume (-70dB to +6dB)

## 重要な実装詳細

### 1. OnePoleIIRフィルタリング
参照版では `alpha=0.8` でアナログ入力をフィルタリング：
```cpp
class OnePoleIIR {
    static constexpr float kDefaultAlpha = 0.8f;
    void update(const float input) {
        const auto output = alpha_ * input + (1.0f - alpha_) * output_;
        output_ = output;
    }
};
```

### 2. スキャン周期
参照版では1msの高速スキャン：
```cpp
scan_period_ms = 1  // 参照版と完全同じ（1ms高速スキャン）
```

### 3. ランダム復帰機能
音量が小さくなりすぎた場合のランダムパラメーター設定：
```cpp
if (fabsf(mixed_out) < 0.01f) {
    fm1.SetFrequency(random(100, 1000));
    fm1.SetIndex(random(0, 20));
    fm1.SetRatio(random(1, 20));
    fm2.SetFrequency(random(100, 1000));
    fm2.SetIndex(random(0, 20));
    fm2.SetRatio(random(1, 20));
}
```

## デバッグ技法

### LEDデバッグパターン
- **3回点滅 + 常時点灯**: 正常起動、シリアル待機中
- **1秒ごと点滅**: 音声処理中

### ステップバイステップ音声デバッグ
1. サイン波で基本動作確認
2. 単一FM合成に切り替え
3. デュアルFM合成に拡張
4. クロスモジュレーション有効化
5. エフェクト（オーバードライブ）追加

## 参照版完全再現のためのチェックリスト

✅ DaisySP Fm2クラス使用  
✅ BUFFER_SIZE=2 → sample_count全処理に変更  
✅ val0=0で最高音質の条件分岐  
✅ 直接乗算クロスモジュレーション  
✅ オーバードライブエフェクト  
✅ dBスケーリングボリューム制御  
✅ ランダム復帰機能  
✅ 1msスキャン周期  
✅ OnePoleIIRフィルタリング (alpha=0.8)  

## トラブルシューティング

### よくある問題と対処法

1. **音が出ない**
   - LEDデバッグパターンを確認
   - サイン波フォールバックでテスト
   - val0を0に設定して確認

2. **音質が参照版と異なる**
   - クロスモジュレーションの直接乗算を確認
   - オーバードライブの順序確認
   - ボリュームのdBスケーリング確認

3. **ノブが反応しない**
   - マルチプレクサーの配線確認
   - スキャン周期設定確認 (1ms)
   - フィルタリング設定確認 (alpha=0.8)

## ビルド・実行手順

### ローカルビルド
```bash
cd products/cross_fm_noise_synth
rm -rf build && mkdir build
cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 -S . -B build
make -C build
```

### UF2転送
1. Picoのリセットボタンを押しながらUSB接続
2. `cross_fm_noise_synth.uf2` をPicoドライブにコピー
3. LEDデバッグパターンで動作確認

## 今後の拡張可能性

- 追加エフェクト（コーラス、リバーブ）
- プリセット保存機能
- MIDI制御対応
- 波形表示機能

---

このノイズシンセは意図的な「破綻」を楽しむ実験的な楽器です。予期しない音の変化や不安定さも含めて設計の一部として楽しんでください。