# アナログマルチプレクサー (74HC4051) 実装ガイド

## 概要

74HC4051アナログマルチプレクサーを使用して、8個のアナログ入力（ノブ・ポテンショメーター）を1つのADCチャンネルで読み取るための実装ガイドです。

## ハードウェア構成

### 74HC4051 ピン配置
```
      74HC4051
     ┌─────────┐
  Y7 │ 1   16 │ VCC (3.3V)
  Y6 │ 2   15 │ Y0
  Y5 │ 3   14 │ Y1
  Y4 │ 4   13 │ Y2
  Y3 │ 5   12 │ Y3
  /EN│ 6   11 │ S0
  GND│ 7   10 │ S1
  S2 │ 8    9 │ COM
     └─────────┘
```

### 接続図
```
Raspberry Pi Pico    →    74HC4051
GP0 (/EN)           →    Pin 6 (/EN)
GP1 (S2)            →    Pin 8 (S2)
GP2 (S1)            →    Pin 10 (S1)
GP3 (S0)            →    Pin 11 (S0)
GP26 (ADC0)         →    Pin 9 (COM)
3.3V                →    Pin 16 (VCC)
GND                 →    Pin 7 (GND)

ポテンショメーター:
Pot 0               →    Pin 15 (Y0)
Pot 1               →    Pin 14 (Y1)
Pot 2               →    Pin 13 (Y2)
Pot 3               →    Pin 12 (Y3)
Pot 4               →    Pin 5 (Y3)
Pot 5               →    Pin 4 (Y4)
Pot 6               →    Pin 3 (Y5)
Pot 7               →    Pin 2 (Y6)
```

## ソフトウェア実装

### 基本クラス構造

```cpp
class AnalogMux {
public:
    struct Config {
        uint pin_enable;      // /ENピン番号
        uint pin_s0;          // S0ピン番号
        uint pin_s1;          // S1ピン番号
        uint pin_s2;          // S2ピン番号
        uint adc_pin;         // ADC入力ピン番号
        uint adc_channel;     // ADCチャンネル番号
        uint scan_period_ms;  // スキャン周期（ミリ秒）
        bool enable_active_low; // Enable信号の極性
    };

    void Init(const Config& config);
    void Update();
    float GetNormalizedValue(int channel);  // 0.0-1.0
    float GetMappedValue(int channel, float min_val, float max_val);
    uint16_t GetRawValue(int channel);      // 0-4095
};
```

### 初期化例

```cpp
AnalogMux g_analog_mux;

void setup() {
    AnalogMux::Config mux_config = {
        .pin_enable = 0,        // GP0 → /EN
        .pin_s0 = 3,           // GP3 → S0
        .pin_s1 = 2,           // GP2 → S1
        .pin_s2 = 1,           // GP1 → S2
        .adc_pin = 26,         // GP26 → ADC0
        .adc_channel = 0,      // ADC channel 0
        .scan_period_ms = 10,  // 10ms周期
        .enable_active_low = true
    };
    
    g_analog_mux.Init(mux_config);
}
```

### メインループでの使用

```cpp
void loop() {
    g_analog_mux.Update();
    
    // 正規化された値を取得 (0.0-1.0)
    float volume = g_analog_mux.GetNormalizedValue(0);
    float frequency = g_analog_mux.GetNormalizedValue(1);
    
    // マップされた値を取得
    float mapped_freq = g_analog_mux.GetMappedValue(1, 100.0f, 2000.0f); // 100Hz-2000Hz
    
    // 生の12bit値を取得 (0-4095)
    uint16_t raw_value = g_analog_mux.GetRawValue(0);
}
```

## スキャン方式

### シーケンシャルスキャン
```cpp
void AnalogMux::Update() {
    static uint32_t last_scan_time = 0;
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    
    if (current_time - last_scan_time >= scan_period_ms_) {
        // 現在のチャンネルを読み取り
        uint16_t raw_value = adc_read();
        values_[current_channel_] = raw_value;
        
        // 次のチャンネルを選択
        current_channel_ = (current_channel_ + 1) % 8;
        SelectChannel(current_channel_);
        
        last_scan_time = current_time;
    }
}
```

### チャンネル選択ロジック
```cpp
void AnalogMux::SelectChannel(uint8_t channel) {
    // 3-bitのチャンネル番号をS2, S1, S0に分解
    gpio_put(pin_s0_, (channel & 0x01) ? 1 : 0);
    gpio_put(pin_s1_, (channel & 0x02) ? 1 : 0);
    gpio_put(pin_s2_, (channel & 0x04) ? 1 : 0);
    
    // Enable信号を活性化
    gpio_put(pin_enable_, enable_active_low_ ? 0 : 1);
}
```

## パラメーターマッピング例

### 線形マッピング
```cpp
float volume = knob_value;  // 0.0-1.0 → 0.0-1.0
```

### 指数マッピング（音量など）
```cpp
float volume = knob_value * knob_value;  // 0.0-1.0 → 0.0-1.0 (二乗カーブ)
```

### 対数マッピング（周波数など）
```cpp
float frequency = 100.0f * powf(20.0f, knob_value);  // 100Hz-2000Hz (対数)
```

### dBスケーリング
```cpp
float volume_db = -70.0f + knob_value * 76.0f;  // -70dB to +6dB
float linear_volume = expf(0.11512925464970229f * volume_db);  // dB to linear
```

## フィルタリング機能

### OnePole IIRフィルタ
```cpp
class OnePoleIIR {
public:
    OnePoleIIR(float alpha = 0.8f) : alpha_(alpha), output_(0.0f) {}
    
    float Process(float input) {
        output_ = alpha_ * input + (1.0f - alpha_) * output_;
        return output_;
    }
    
private:
    float alpha_;   // フィルタ係数 (0.0-1.0)
    float output_;  // フィルタ出力
};
```

### 使用例
```cpp
// チャンネルごとにフィルタを用意
OnePoleIIR filters_[8];

void ProcessValues() {
    for (int i = 0; i < 8; i++) {
        float raw_normalized = raw_values_[i] / 4095.0f;
        filtered_values_[i] = filters_[i].Process(raw_normalized);
    }
}
```

## パフォーマンス考慮事項

### スキャン周期の選択
- **1ms**: 高応答性、ただしCPU負荷が高い
- **10ms**: 一般的な用途に適する
- **50ms**: 低負荷、ゆっくりした変化に適する

### メモリ使用量
```cpp
// 最小構成での使用量
struct AnalogMux {
    uint16_t values_[8];        // 16 bytes
    OnePoleIIR filters_[8];     // 64 bytes (8 * 8 bytes)
    // 設定データなど...      // ~32 bytes
    // 合計: ~112 bytes
};
```

## トラブルシューティング

### よくある問題

1. **値が不安定**
   - バイパスコンデンサーを追加 (0.1μF)
   - グラウンドループを確認
   - スキャン周期を長くする

2. **チャンネルが切り替わらない**
   - 配線を確認（S0, S1, S2）
   - Enable信号の極性を確認
   - 電源電圧を確認 (3.3V)

3. **応答が遅い**
   - スキャン周期を短くする
   - フィルタ係数を上げる (alpha > 0.8)

4. **ノイズが多い**
   - アナログ電源ラインにフィルタリング追加
   - デジタル/アナロググラウンド分離
   - シールドケーブル使用

### デバッグ用出力
```cpp
void DebugPrintValues() {
    printf("Mux Values: ");
    for (int i = 0; i < 8; i++) {
        printf("Ch%d=%d ", i, GetRawValue(i));
    }
    printf("\n");
}
```

## 実装例: sine_wave_i2s_32b

```cpp
// samples/sine_wave_i2s_32b/sine_wave.cpp での使用例
enum KnobFunction {
    KNOB_VOLUME = 0,     // ノブ0: マスターボリューム
    KNOB_LEFT_FREQ = 1,  // ノブ1: 左チャンネル周波数
    KNOB_RIGHT_FREQ = 2, // ノブ2: 右チャンネル周波数
    KNOB_UNUSED3 = 3,    // ノブ3-7: 未使用
    // ...
};

void main_loop() {
    g_analog_mux.Update();
    
    float knob_volume = g_analog_mux.GetNormalizedValue(KNOB_VOLUME);
    float knob_left_freq = g_analog_mux.GetNormalizedValue(KNOB_LEFT_FREQ);
    float knob_right_freq = g_analog_mux.GetNormalizedValue(KNOB_RIGHT_FREQ);
    
    // パラメーター適用
    uint new_vol = (uint)(knob_volume * 32);
    uint32_t new_step0 = 0x10000 + (uint32_t)(knob_left_freq * (0x200000 - 0x10000));
    uint32_t new_step1 = 0x10000 + (uint32_t)(knob_right_freq * (0x200000 - 0x10000));
    
    // 値の更新
    vol = new_vol;
    step0 = new_step0;
    step1 = new_step1;
}
```

## 拡張可能性

### 16チャンネル対応 (74HC4067)
```cpp
// 4-bit selection for 16 channels
struct Config {
    uint pin_s3;  // 追加の選択ピン
    // ...
};
```

### 複数マルチプレクサー
```cpp
AnalogMux mux1, mux2;  // 最大16チャンネル
```

### 外部ADC連携
```cpp
// MCP3008などの外部ADC使用時
uint16_t ReadExternalADC(uint8_t channel);
```