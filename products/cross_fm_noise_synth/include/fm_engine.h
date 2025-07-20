/**
 * @file fm_engine.h
 * @brief Cross FM Noise Synthesizer - FMエンジン（スタブ）
 */

#ifndef FM_ENGINE_H
#define FM_ENGINE_H

#include "synth_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// スタブ関数宣言
void fm_engine_init(FMEngine *engine);
int32_t fm_engine_process(FMEngine *engine);

#ifdef __cplusplus
}
#endif

#endif // FM_ENGINE_H