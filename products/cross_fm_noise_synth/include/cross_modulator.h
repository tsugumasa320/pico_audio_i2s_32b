/**
 * @file cross_modulator.h
 * @brief Cross FM Noise Synthesizer - クロスモジュレーター（スタブ）
 */

#ifndef CROSS_MODULATOR_H
#define CROSS_MODULATOR_H

#include "synth_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// スタブ関数宣言
void cross_modulator_init(CrossModulator *mod);
int32_t cross_modulator_process(CrossModulator *mod, int32_t fm_input, int32_t noise_input);

#ifdef __cplusplus
}
#endif

#endif // CROSS_MODULATOR_H