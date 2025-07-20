/**
 * @file synth_config.h
 * @brief Cross FM Noise Synthesizer - 設定とデータ構造
 */

#ifndef SYNTH_CONFIG_H
#define SYNTH_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

// ===== ハードウェア設定 =====
#define SYNTH_SAMPLE_RATE       44100
#define SYNTH_BUFFER_SIZE       256
#define SYNTH_MAX_POLYPHONY     4

// GPIO ピン定義
#define PIN_ENCODER_A           2
#define PIN_ENCODER_B           3
#define PIN_ENCODER_SW          4
#define PIN_BUTTON_PRESET       5
#define PIN_BUTTON_MENU         6
#define PIN_LED_STATUS          25

// ADC ピン定義（アナログコントロール）
#define ADC_FM_RATIO            0
#define ADC_FM_DEPTH            1
#define ADC_NOISE_LEVEL         2
#define ADC_CROSS_MOD           3

// ===== FMエンジン設定 =====
#define FM_OPERATORS            4
#define FM_MAX_RATIO            16.0f
#define FM_MAX_FEEDBACK         1.0f

// ===== ノイズ設定 =====
typedef enum {
    NOISE_WHITE,
    NOISE_PINK,
    NOISE_BROWN,
    NOISE_BLUE,
    NOISE_COUNT
} NoiseType;

// ===== データ構造 =====

/**
 * @brief FMオペレーター
 */
typedef struct {
    float frequency;        // 周波数
    float ratio;           // 基音に対する比率
    float amplitude;       // 振幅
    float feedback;        // フィードバック量
    float phase;           // 現在の位相
} FMOperator;

/**
 * @brief FMエンジン状態
 */
typedef struct {
    FMOperator operators[FM_OPERATORS];
    float base_frequency;   // 基音周波数
    uint8_t algorithm;      // アルゴリズム番号
    bool enabled;
} FMEngine;

/**
 * @brief ノイズジェネレーター状態
 */
typedef struct {
    NoiseType type;
    float level;
    uint32_t seed;          // 乱数シード
    float filter_state;     // フィルター状態
    bool enabled;
} NoiseGenerator;

/**
 * @brief クロスモジュレーター状態
 */
typedef struct {
    float depth;            // モジュレーション深度
    float rate;             // モジュレーション速度
    float phase;            // LFO位相
    bool enabled;
} CrossModulator;

/**
 * @brief UI制御状態
 */
typedef struct {
    uint8_t current_preset;
    uint8_t current_parameter;
    bool menu_mode;
    uint32_t last_encoder_time;
} UIController;

/**
 * @brief プリセット管理
 */
#define MAX_PRESETS 16

typedef struct {
    FMEngine fm_engine;
    NoiseGenerator noise_gen;
    CrossModulator cross_mod;
    char name[16];
} Preset;

typedef struct {
    Preset presets[MAX_PRESETS];
    uint8_t current_preset;
    bool dirty;             // 変更フラグ
} PresetManager;

/**
 * @brief シンセサイザー全体状態
 */
typedef struct {
    FMEngine fm_engine;
    NoiseGenerator noise_gen;
    CrossModulator cross_mod;
    UIController ui;
    PresetManager preset_mgr;
    
    // パフォーマンス統計
    uint32_t cpu_usage;
    uint32_t buffer_underruns;
} SynthState;

#endif // SYNTH_CONFIG_H