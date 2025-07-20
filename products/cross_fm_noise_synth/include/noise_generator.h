/**
 * @file noise_generator.h
 * @brief Cross FM Noise Synthesizer - ノイズジェネレーター（スタブ）
 */

#ifndef NOISE_GENERATOR_H
#define NOISE_GENERATOR_H

#include "synth_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// スタブ関数宣言
void noise_generator_init(NoiseGenerator *gen);
int32_t noise_generator_process(NoiseGenerator *gen);

#ifdef __cplusplus
}
#endif

#endif // NOISE_GENERATOR_H