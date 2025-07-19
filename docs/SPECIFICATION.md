# 🎵 Pico Audio I2S 32-bit Library Specification

**Version**: 1.0  
**Date**: 2025-01-19  
**Target Platform**: Raspberry Pi Pico / Pico 2 (RP2040/RP2350)

## 📋 目次

- [概要](#概要)
- [システム仕様](#システム仕様)
- [ハードウェア要件](#ハードウェア要件)
- [ソフトウェアアーキテクチャ](#ソフトウェアアーキテクチャ)
- [API仕様](#API仕様)
- [パフォーマンス仕様](#パフォーマンス仕様)
- [電気的特性](#電気的特性)
- [制限事項](#制限事項)

## 🎯 概要

### プロジェクト概要
Raspberry Pi Pico/Pico 2向けの高性能32bit I2Sオーディオ出力ライブラリです。PIO（Programmable I/O）を活用した正確なタイミング制御とDMAによる効率的なデータ転送により、プロフェッショナル品質のオーディオ出力を実現します。

### 設計目標
- **高音質**: 32bit PCM対応によるプロフェッショナル品質
- **低レイテンシ**: DMAベースストリーミングによる最小限の遅延
- **高効率**: CPU使用率5%以下でのオーディオ処理
- **柔軟性**: 8kHz〜192kHzの広範囲サンプリング周波数対応
- **拡張性**: デュアルコア処理とマルチプラットフォーム対応

## 🔧 システム仕様

### サポートプラットフォーム
| プラットフォーム | MCU | 対応状況 | 動作確認済みバージョン |
|-----------------|-----|----------|----------------------|
| Raspberry Pi Pico | RP2040 | ✅ Full Support | pico-sdk 2.1.1 |
| Raspberry Pi Pico 2 | RP2350 | ✅ Full Support | pico-sdk 2.1.1 |

### オーディオ仕様
| 項目 | 仕様値 | 備考 |
|------|--------|------|
| **対応PCMフォーマット** | 16-bit, 32-bit signed | リトルエンディアン |
| **チャンネル数** | 2ch (ステレオ) | モノラル→ステレオ変換対応 |
| **サンプリング周波数** | 8kHz ～ 192kHz | 動的変更可能 |
| **ビットクロック精度** | ±0.1% | PIO クロック精度に依存 |
| **S/N比** | DAC依存 | PCM5102: 112dB typ. |
| **THD+N** | DAC依存 | PCM5102: -93dB typ. |

### I2Sフォーマット準拠
- **I2S標準**: Philips I2S specification準拠
- **データ配置**: MSBファースト
- **LRCLK極性**: 0=Right, 1=Left
- **データ有効タイミング**: LRCLK遷移後のBCLK立ち下がりエッジ

## 🔌 ハードウェア要件

### 必要なハードウェアリソース
| リソース | 使用量 | 設定可能 |
|----------|--------|----------|
| **GPIO Pin** | 3本 | ✅ 任意のGPIO |
| **PIO State Machine** | 1個 | ✅ PIO0/PIO1選択可 |
| **DMA Channel** | 2個 | ✅ 任意のチャンネル |
| **メモリ** | ~14KB | バッファサイズ依存 |

### GPIO ピンアサイン（デフォルト）
| 信号名 | GPIO | 方向 | 説明 |
|--------|------|------|------|
| **SDATA** | GP18 | OUT | シリアルオーディオデータ |
| **BCLK** | GP16 | OUT | ビットクロック |
| **LRCLK** | GP17 | OUT | 左右チャンネルクロック |

### 対応DACチップ
| チップ | 解像度 | サンプリング周波数 | 動作確認 |
|--------|--------|-------------------|----------|
| **PCM5102** | 32-bit | 8kHz～192kHz | ✅ |
| **ES9023** | 24-bit | 8kHz～192kHz | ✅ |
| **PCM1808** | 24-bit | 8kHz～96kHz | 🔄 |

## 🏗️ ソフトウェアアーキテクチャ

### システム構成図
```
┌─────────────────┐    ┌──────────────┐    ┌──────────────┐
│   Application   │    │  Core1 Opt.  │    │   Hardware   │
│                 │    │  Processing  │    │              │
├─────────────────┤    ├──────────────┤    ├──────────────┤
│ Audio Generation│    │ Audio Callback│   │     DAC      │
│                 │    │              │    │   PCM5102    │
└─────────┬───────┘    └──────┬───────┘    └──────▲───────┘
          │                   │                   │
          ▼                   ▼                   │
┌─────────────────────────────────────────────────┼───────┐
│               Pico Audio I2S Library            │       │
├─────────────────┬───────────────────────────────┼───────┤
│  Buffer Pool    │         DMA Engine            │ PIO   │
│  Management     │                               │ I2S   │
│                 │  ┌─────────┐  ┌─────────┐    │ State │
│ ┌─────────────┐ │  │Channel 0│  │Channel 1│    │Machine│
│ │   Buffer    │ │  │(Ping)   │  │(Pong)   │    │       │
│ │    Pool     │ │  └─────────┘  └─────────┘    │       │
│ └─────────────┘ │                               │       │
└─────────────────┴───────────────────────────────┼───────┘
                                                  │
                                                  ▼
                                          ┌──────────────┐
                                          │   I2S Bus    │
                                          │ SDATA/BCLK/  │
                                          │   LRCLK      │
                                          └──────────────┘
```

### モジュール構成
| モジュール | ファイル | 責任 |
|-----------|----------|------|
| **PIO Controller** | `audio_i2s.pio` | I2S信号生成 |
| **DMA Manager** | `audio_i2s.c` | データ転送制御 |
| **Buffer Manager** | `pico_audio_32b/audio.cpp` | バッファプール管理 |
| **Format Converter** | `sample_conversion.h` | フォーマット変換 |
| **Public API** | `audio_i2s.h` | 外部インターフェース |

### データフロー
```
[App] → [Buffer Pool] → [DMA] → [PIO] → [I2S Bus] → [DAC]
  ↑         ↓            ↓       ↓        ↓          ↓
Generate  Queue       Transfer  Serialize  Transmit  Convert
Audio     Buffers     to PIO    Bits      Signals   to Analog
```

## 📡 API仕様

### 初期化フロー
```c
// 1. オーディオフォーマット設定
audio_format_t format = {
    .sample_freq = 44100,
    .format = AUDIO_BUFFER_FORMAT_PCM_S32,
    .channel_count = 2
};

// 2. ハードウェア設定
audio_i2s_config_t config = {
    .data_pin = 18,
    .clock_pin_base = 16,
    .dma_channel0 = 0,
    .dma_channel1 = 1,
    .pio_sm = 0
};

// 3. I2S初期化
const audio_format_t *output_format = audio_i2s_setup(&format, &format, &config);

// 4. バッファプール作成
audio_buffer_pool_t *pool = audio_new_producer_pool(&format, 3, 1024);

// 5. ストリーミング開始
audio_i2s_connect(pool);
audio_i2s_set_enabled(true);
```

### 主要API関数

#### `audio_i2s_setup()`
```c
const audio_format_t *audio_i2s_setup(
    const audio_format_t *input_format,
    const audio_format_t *output_format, 
    const audio_i2s_config_t *config
);
```
**機能**: I2Sハードウェアの初期化とPIO設定  
**戻り値**: 実際の出力フォーマット（NULLは失敗）  
**エラー条件**: PIO使用不可、無効なパラメータ

#### `audio_i2s_set_frequency()`
```c
void audio_i2s_set_frequency(uint32_t sample_freq);
```
**機能**: 実行時サンプリング周波数変更  
**制限**: 8kHz～192kHz範囲内  
**注意**: 変更中は一時的な音途切れが発生する可能性

## ⚡ パフォーマンス仕様

### CPU使用率（RP2040 @ 125MHz）
| サンプリング周波数 | 解像度 | CPU使用率 | メモリ使用量 |
|-------------------|--------|-----------|-------------|
| 44.1kHz | 16-bit | ~2% | ~7KB |
| 44.1kHz | 32-bit | ~5% | ~14KB |
| 96kHz | 32-bit | ~8% | ~28KB |
| 192kHz | 32-bit | ~15% | ~56KB |

### レイテンシ仕様
| 要素 | 時間 | 説明 |
|------|------|------|
| **DMA転送遅延** | <1ms | ハードウェア依存 |
| **バッファリング遅延** | 20-30ms | バッファサイズ依存 |
| **PIO処理遅延** | <10µs | ハードウェア処理 |
| **総合レイテンシ** | <35ms | アプリケーション依存 |

### メモリフットプリント
```
基本ライブラリ: ~8KB (Flash)
実行時メモリ: buffer_count × buffer_size × 8 bytes
             + ~2KB (制御構造体)

例: 3バッファ × 1156サンプル × 8バイト = ~28KB
```

## 🔋 電気的特性

### 信号レベル
| 信号 | 電圧レベル | 駆動能力 | インピーダンス |
|------|-----------|----------|---------------|
| **SDATA** | 3.3V CMOS | 8mA | 50Ω |
| **BCLK** | 3.3V CMOS | 8mA | 50Ω |
| **LRCLK** | 3.3V CMOS | 8mA | 50Ω |

### クロック仕様
| パラメータ | 値 | 備考 |
|-----------|-----|------|
| **最大BCLK周波数** | 12.288MHz | 192kHz×32bit×2ch |
| **BCLK jitter** | <±50ppm | PIO clock精度 |
| **LRCLK精度** | ±0.01% | 長期安定性 |

### 消費電力
| 動作状態 | 消費電流 | 備考 |
|---------|----------|------|
| **Idle** | +50µA | PIO動作分 |
| **44.1kHz動作** | +200µA | DMA + PIO |
| **192kHz動作** | +500µA | 高周波数動作 |

## ⚠️ 制限事項

### ハードウェア制限
- **PIOプログラムサイズ**: 最大8命令（現在6命令使用）
- **同時PIOプログラム**: 最大4個（1つのPIOインスタンス当たり）
- **DMAチャンネル**: 最大12個利用可能（2個使用）
- **GPIO制限**: 隣接する3ピンが必要

### ソフトウェア制限  
- **マルチインスタンス**: 現在1つのI2S出力のみサポート
- **入力対応**: I2S入力は未サポート
- **リアルタイム性**: DMA IRQ優先度に依存
- **エラー処理**: バッファアンダーランの自動回復は未実装

### 環境制限
- **温度範囲**: -40°C ～ +85°C（RP2040/RP2350仕様準拠）
- **電源電圧**: 1.8V ～ 3.6V（GPIO: 3.3V）
- **EMI考慮**: 高周波クロックによるEMI発生の可能性

## 🔮 将来の拡張予定

### Phase 2
- [ ] I2S入力（ADC）対応
- [ ] 複数I2S出力の同期
- [ ] エラー処理の強化
- [ ] リアルタイム周波数変更の改善

### Phase 3  
- [ ] TDM（Time Division Multiplexing）対応
- [ ] SPDIF出力対応
- [ ] ハードウェアボリューム制御
- [ ] 低電力モード対応

### Phase 4
- [ ] USB Audio Class対応
- [ ] Bluetooth Audio Profile対応
- [ ] DSP機能統合
- [ ] マルチチャンネル対応（最大8ch）

---

**ドキュメント更新**: このドキュメントはライブラリの更新に合わせて継続的に更新されます。  
**技術サポート**: 詳細な技術情報は[API Documentation](API.md)を参照してください。