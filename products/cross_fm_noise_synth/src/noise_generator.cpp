/**
 * @file noise_generator.cpp
 * @brief Cross FM Noise Synthesizer - ノイズジェネレーター実装（DaisySPベース）
 */

#include "noise_generator.h"
#include <daisysp.h>

using namespace daisysp;

// DaisySPのノイズジェネレーターインスタンス
static WhiteNoise white_noise;
static ClockedNoise clocked_noise;
static bool initialized = false;

// ピンクノイズ用のフィルター状態
static float pink_filter_state[4] = {0.0f, 0.0f, 0.0f, 0.0f};

// ブラウンノイズ用の積分器
static float brown_integrator = 0.0f;

void noise_generator_init(NoiseGenerator *generator) {
    if (!generator) return;
    
    // DaisySPのノイズジェネレーターを初期化
    white_noise.Init();
    clocked_noise.Init(SYNTH_SAMPLE_RATE);
    clocked_noise.SetFreq(1000.0f);  // クロック周波数
    
    // ノイズジェネレーター状態を初期化
    generator->type = NOISE_WHITE;
    generator->level = 0.5f;
    generator->seed = 12345;
    generator->filter_state = 0.0f;
    generator->enabled = true;
    
    // フィルター状態をリセット
    for (int i = 0; i < 4; i++) {
        pink_filter_state[i] = 0.0f;
    }
    brown_integrator = 0.0f;
    
    initialized = true;
}

static float generate_pink_noise() {
    // Paul Kellet の Pink Noise アルゴリズム
    float white = white_noise.Process();
    
    pink_filter_state[0] = 0.99886f * pink_filter_state[0] + white * 0.0555179f;
    pink_filter_state[1] = 0.99332f * pink_filter_state[1] + white * 0.0750759f;
    pink_filter_state[2] = 0.96900f * pink_filter_state[2] + white * 0.1538520f;
    pink_filter_state[3] = 0.86650f * pink_filter_state[3] + white * 0.3104856f;
    
    return pink_filter_state[0] + pink_filter_state[1] + pink_filter_state[2] + pink_filter_state[3] + white * 0.5362f;
}

static float generate_brown_noise() {
    // ブラウンノイズ（赤ノイズ）= ホワイトノイズの積分
    float white = white_noise.Process();
    brown_integrator += white * 0.02f;
    
    // オーバーフローを防ぐためにクリップ
    if (brown_integrator > 1.0f) brown_integrator = 1.0f;
    if (brown_integrator < -1.0f) brown_integrator = -1.0f;
    
    return brown_integrator;
}

static float generate_blue_noise() {
    // ブルーノイズ（高域強調）= ホワイトノイズの微分近似
    static float prev_sample = 0.0f;
    float current = white_noise.Process();
    float blue = current - prev_sample;
    prev_sample = current;
    return blue * 2.0f;  // ゲイン調整
}

int32_t noise_generator_process(NoiseGenerator *generator) {
    if (!generator || !generator->enabled || !initialized) return 0;
    
    float output = 0.0f;
    
    // ノイズタイプに応じて生成
    switch (generator->type) {
        case NOISE_WHITE:
            output = white_noise.Process();
            break;
            
        case NOISE_PINK:
            output = generate_pink_noise();
            break;
            
        case NOISE_BROWN:
            output = generate_brown_noise();
            break;
            
        case NOISE_BLUE:
            output = generate_blue_noise();
            break;
            
        default:
            output = white_noise.Process();
            break;
    }
    
    // レベル調整
    output *= generator->level;
    
    // 32bit PCMに変換
    return (int32_t)(output * 2147483647.0f);
}