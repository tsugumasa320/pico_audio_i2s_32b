/**
 * @file preset_manager.h
 * @brief Cross FM Noise Synthesizer - プリセット管理（スタブ）
 */

#ifndef PRESET_MANAGER_H
#define PRESET_MANAGER_H

#include "synth_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// スタブ関数宣言
void preset_manager_init(PresetManager *mgr);
void preset_manager_update(PresetManager *mgr, SynthState *state);

#ifdef __cplusplus
}
#endif

#endif // PRESET_MANAGER_H