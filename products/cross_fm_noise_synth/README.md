# Cross FM Noise Synthesizer

クロスFMとノイズを組み合わせた実験的なシンセサイザー

## 概要

- **4オペレータークロスFM**: 複雑な倍音を生成
- **マルチノイズソース**: ホワイト/ピンク/ブラウンノイズ
- **クロスモジュレーション**: FM出力とノイズの相互変調
- **リアルタイム制御**: アナログコントロールによる即座のパラメーター変更
- **プリセット管理**: 16種類のプリセット保存・呼び出し

## ハードウェア要件

- Raspberry Pi Pico または Pico 2
- PCM5102 32-bit I2S DAC
- アナログコントロール（ポテンショメーター × 4）
- ロータリーエンコーダー × 1
- プッシュボタン × 2
- ステータスLED × 1

## ビルド方法

```bash
cd products/cross_fm_noise_synth
mkdir build && cd build
cmake ..
make -j4
```

## 接続図

### I2S DAC接続
- GP16: BCK (Bit Clock)
- GP17: LRCK (Left/Right Clock)  
- GP18: SDO (Serial Data Output)

### アナログコントロール
- ADC0: FM Ratio
- ADC1: FM Depth
- ADC2: Noise Level
- ADC3: Cross Modulation

### デジタルコントロール
- GP2/3: ロータリーエンコーダー
- GP4: エンコーダープッシュボタン
- GP5: プリセットボタン
- GP6: メニューボタン
- GP25: ステータスLED

## 開発状況

- [x] ディレクトリ構造作成
- [x] 基本ファイル構造
- [x] サイン波ベーススタブ実装
- [ ] FMエンジン実装
- [ ] ノイズジェネレーター実装
- [ ] クロスモジュレーション実装
- [ ] UI制御実装
- [ ] プリセット管理実装
- [ ] ハードウェア設計
- [ ] テスト・検証

## ライセンス

このプロジェクトはMITライセンスの下で公開されています。