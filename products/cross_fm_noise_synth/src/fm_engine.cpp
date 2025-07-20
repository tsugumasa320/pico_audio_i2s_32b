/**
 * @file fm_engine.cpp
 * @brief Cross FM Noise Synthesizer - FMエンジン実装（DaisySPベース）
 */

#include "fm_engine.h"
#include <daisysp.h>
#include <cmath>

using namespace daisysp;

// DaisySPのFMオシレーターインスタンス
static Fm2 fm_osc1, fm_osc2;
static bool initialized = false;

void fm_engine_init(FMEngine *engine) {
    if (!engine) return;
    
    // DaisySPのFMオシレーターを初期化
    fm_osc1.Init(SYNTH_SAMPLE_RATE);
    fm_osc2.Init(SYNTH_SAMPLE_RATE);
    
    // 初期パラメーター設定
    fm_osc1.SetFrequency(440.0f);
    fm_osc1.SetRatio(2.0f);
    fm_osc1.SetIndex(5.0f);
    
    fm_osc2.SetFrequency(330.0f);
    fm_osc2.SetRatio(1.5f);
    fm_osc2.SetIndex(3.0f);
    
    // エンジン状態を初期化
    for (int i = 0; i < FM_OPERATORS; i++) {
        engine->operators[i].frequency = 440.0f + i * 110.0f;
        engine->operators[i].ratio = 1.0f + i * 0.5f;
        engine->operators[i].amplitude = 0.8f / FM_OPERATORS;
        engine->operators[i].feedback = 0.0f;
        engine->operators[i].phase = 0.0f;
    }
    
    engine->base_frequency = 440.0f;
    engine->algorithm = 0;
    engine->enabled = true;
    initialized = true;
}

int32_t fm_engine_process(FMEngine *engine) {
    if (!engine || !engine->enabled || !initialized) return 0;
    
    // DaisySPのFMオシレーターで音声生成
    float out1 = fm_osc1.Process();
    float out2 = fm_osc2.Process();
    
    // クロスモジュレーション（互いの出力で変調）
    fm_osc1.SetFrequency(engine->base_frequency + out2 * 50.0f);
    fm_osc2.SetFrequency(engine->base_frequency * 0.75f + out1 * 30.0f);
    
    // ミックス
    float mixed_output = (out1 + out2) * 0.5f * 0.3f;  // 音量調整
    
    // 32bit PCMに変換
    return (int32_t)(mixed_output * 2147483647.0f);
}