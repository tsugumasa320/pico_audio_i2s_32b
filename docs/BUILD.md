# ビルド手順書

## 📋 目次
- [環境構築](#環境構築)
- [依存関係](#依存関係)
- [ビルド方法](#ビルド方法)
- [アップロード方法](#アップロード方法)
- [トラブルシューティング](#トラブルシューティング)

## 🛠️ 環境構築

### macOS
```bash
# 1. Homebrew をインストール (未インストールの場合)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 2. ARM GCC クロスコンパイラをインストール
brew install arm-none-eabi-gcc

# 3. CMake をインストール
brew install cmake

# 4. Git をインストール
brew install git
```

### Windows
1. **Visual Studio 2022** の "C++ によるデスクトップ開発" をインストール
2. **CMake** をインストール: https://cmake.org/download/
3. **ARM GCC** をインストール: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
4. **Git** をインストール: https://git-scm.com/download/win

### Linux (Ubuntu/Debian)
```bash
# 1. 必要なパッケージをインストール
sudo apt update
sudo apt install build-essential cmake git

# 2. ARM GCC クロスコンパイラをインストール
sudo apt install gcc-arm-none-eabi
```

## 📦 依存関係

### Raspberry Pi Pico SDK のセットアップ
```bash
# 1. 作業ディレクトリを作成
mkdir ~/pico-development
cd ~/pico-development

# 2. Pico SDK をクローン (推奨バージョン: 2.1.1)
git clone -b 2.1.1 https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
cd ..

# 3. このプロジェクトをクローン
git clone https://github.com/tsugumasa320/pico_audio_i2s_32b.git
```

### 環境変数の設定

#### macOS/Linux (.bashrc または .zshrc に追加)
```bash
export PICO_SDK_PATH=~/pico-development/pico-sdk
```

#### Windows (システム環境変数に追加)
```
PICO_SDK_PATH=C:\pico-development\pico-sdk
```

**注意**: 
- `PICO_EXTRAS_PATH` は自動的にプロジェクト内の `libs/pico-extras` を使用します
- `PICO_EXAMPLES_PATH` は不要です

## 🔨 ビルド方法

### VS Code を使用する場合（推奨）

1. **VS Code でプロジェクトを開く**
   ```bash
   cd ~/pico-development/pico_audio_i2s_32b
   code .
   ```

2. **タスクを実行**
   - `Ctrl+Shift+P` → `Tasks: Run Task` を選択
   - 使用可能なタスク:
     - `Configure CMake` - CMake 設定
     - `Build Project` - ライブラリ全体のビルド
     - `Build Sample (sine_wave_i2s_32b)` - サンプルプロジェクトのビルド
     - `Upload to Pico (picotool)` - Pico へアップロード
     - `Clean Build` - ビルドファイルのクリア

### コマンドラインを使用する場合

#### Raspberry Pi Pico (RP2040) 用
```bash
cd ~/pico-development/pico_audio_i2s_32b/samples/sine_wave_i2s_32b
mkdir build && cd build

# macOS/Linux
cmake -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
      -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
      -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc ..
make -j4

# Windows (Developer Command Prompt for VS 2022)
cmake -G "NMake Makefiles" \
      -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
      -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
      -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc ..
nmake
```

#### Raspberry Pi Pico 2 (RP2350) 用
```bash
cd ~/pico-development/pico_audio_i2s_32b/samples/sine_wave_i2s_32b
mkdir build && cd build

# macOS/Linux
cmake -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
      -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
      -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc \
      -DPICO_PLATFORM=rp2350 \
      -DPICO_BOARD=pico2 ..
make -j4

# Windows (Developer Command Prompt for VS 2022)
cmake -G "NMake Makefiles" \
      -DCMAKE_C_COMPILER=arm-none-eabi-gcc \
      -DCMAKE_CXX_COMPILER=arm-none-eabi-g++ \
      -DCMAKE_ASM_COMPILER=arm-none-eabi-gcc \
      -DPICO_PLATFORM=rp2350 \
      -DPICO_BOARD=pico2 ..
nmake
```

## 📤 アップロード方法

### 方法 1: BOOTSEL モード（推奨）
1. Pico の **BOOTSEL** ボタンを押しながら USB ケーブルで PC に接続
2. `RPI-RP2` (Pico) または `RP2350` (Pico 2) ドライブが表示される
3. ビルドした `.uf2` ファイルを該当ドライブにコピー

```bash
# ファイルの場所例
# Pico: ~/pico-development/pico_audio_i2s_32b/samples/sine_wave_i2s_32b/build/sine_wave_i2s_32b.uf2

# macOS例
cp build/sine_wave_i2s_32b.uf2 /Volumes/RPI-RP2/

# Linux例
cp build/sine_wave_i2s_32b.uf2 /media/username/RPI-RP2/

# Windows例
copy build\sine_wave_i2s_32b.uf2 E:\
```

### 方法 2: picotool を使用
```bash
# picotool でアップロード
picotool load build/sine_wave_i2s_32b.uf2 -fx
```

### 方法 3: VS Code タスク
1. `Ctrl+Shift+P` → `Tasks: Run Task`
2. `Upload to Pico (picotool)` を選択

## 🐛 トラブルシューティング

### よくある問題と解決方法

#### 1. `arm-none-eabi-gcc: command not found`
**原因**: ARM GCC クロスコンパイラがインストールされていない
**解決方法**: 
- macOS: `brew install arm-none-eabi-gcc`
- Ubuntu: `sudo apt install gcc-arm-none-eabi`
- Windows: ARM GCC をダウンロードしてインストール

#### 2. `PICO_SDK_PATH is not set`
**原因**: 環境変数が設定されていない
**解決方法**: [環境変数の設定](#環境変数の設定) を参照

#### 3. `unknown directive .syntax unified`
**原因**: macOS の標準アセンブラが ARM アセンブリを理解できない
**解決方法**: ARM GCC クロスコンパイラを正しく指定してビルド

#### 4. VS Code で Intellisense が働かない
**原因**: C/C++ 拡張の設定が不適切
**解決方法**: 
1. C/C++ 拡張をインストール
2. `Ctrl+Shift+P` → `C/C++: Edit Configurations (JSON)` で設定を確認

#### 5. ビルドファイルが見つからない
**原因**: ビルドディレクトリが作成されていない
**解決方法**: 
```bash
# ビルドディレクトリをクリアして再作成
rm -rf build
mkdir build && cd build
cmake ..
```

### デバッグのヒント

1. **ビルドログを確認**: エラーメッセージの詳細を読む
2. **環境変数を確認**: `echo $PICO_SDK_PATH` で設定を確認
3. **パスを確認**: ファイルパスにスペースや特殊文字が含まれていないか確認
4. **権限を確認**: ファイルの読み書き権限があるか確認

### サポート情報

- **公式ドキュメント**: [Getting Started with Raspberry Pi Pico](https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
- **GitHub Issues**: プロジェクトの Issue で質問可能
- **Raspberry Pi Forum**: コミュニティサポート