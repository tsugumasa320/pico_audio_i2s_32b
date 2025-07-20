# セットアップガイド

このプロジェクトをクローンして使用するための手順です。

## 1. 前提条件

### 必要な環境変数
プロジェクトを使用する前に、以下の環境変数を設定してください：

```bash
# Pico SDKへのパス（重要：この環境変数のみ設定が必要です）
export PICO_SDK_PATH="/path/to/your/pico-sdk"
```

**注意:** `PICO_EXTRAS_PATH`と`PICO_EXAMPLES_PATH`は設定不要です。これらはプロジェクト内に含まれています。

### 環境変数設定例

#### macOS/Linux (.bashrc または .zshrc に追加)
```bash
# Pico SDK Path
export PICO_SDK_PATH="$HOME/.pico-sdk/sdk/2.1.1"
```

#### Windows (環境変数の設定)
```cmd
set PICO_SDK_PATH=C:\path\to\pico-sdk
```

## 2. プロジェクトのクローン

```bash
git clone https://github.com/tsugumasa320/pico_audio_i2s_32b.git
cd pico_audio_i2s_32b
```

## 3. ビルド手順

### コマンドライン（推奨）

#### Raspberry Pi Pico (RP2040)
```bash
cd samples/sine_wave_i2s_32b
mkdir build && cd build
cmake ..
make -j4
```

#### Raspberry Pi Pico 2 (RP2350)
```bash
cd samples/sine_wave_i2s_32b
mkdir build && cd build
cmake -DPICO_PLATFORM=rp2350 -DPICO_BOARD=pico2 ..
make -j4
```

### VS Code
1. VS Codeでプロジェクトのルートディレクトリを開く
2. `samples/sine_wave_i2s_32b/`フォルダに移動
3. Ctrl+Shift+P → "Tasks: Run Task" → "Clean Build & Upload (Pico 2)" を選択

## 4. アップロード

### 手動アップロード
1. Picoをブートローダーモード（BOOTSELボタン押しながら接続）で接続
2. 生成された`.uf2`ファイルをPicoドライブにコピー

### picotoolを使用（推奨）
```bash
picotool load build/sine_wave_i2s_32b.uf2 -fx
```

## 5. ハードウェア接続

### デフォルトピン配置
- **GP16**: BCK (ビットクロック)
- **GP17**: LRCK (左右クロック) 
- **GP18**: SDO (シリアルデータ出力)
- **VBUS**: DAC用5V電源

### 対応DAC
- PCM5102 (32bit)
- ES9023 (24bit)

## 6. トラブルシューティング

### ビルドエラー
- `PICO_SDK_PATH`が正しく設定されているか確認
- パスに日本語や空白文字が含まれていないか確認

### アップロードエラー
- RP2040とRP2350のプラットフォーム設定が正しいか確認
- `rm -rf build`でキャッシュクリア後に再ビルド

### 音声が出ない
- ハードウェア接続を確認
- DACの電源（VBUS 5V）が接続されているか確認
- 音量設定が適切か確認（デフォルト：vol=8）

## 7. 開発に関する詳細

さらに詳しい開発情報については以下のファイルを参照してください：
- `CLAUDE.md` - 開発ノウハウと重要事項
- `docs/SPECIFICATION.md` - 技術仕様書
- `README.md` - プロジェクト概要