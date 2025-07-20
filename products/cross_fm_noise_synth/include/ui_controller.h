/**
 * @file ui_controller.h
 * @brief Cross FM Noise Synthesizer - UI制御（スタブ）
 */

#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include "synth_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// スタブ関数宣言
void ui_controller_init(UIController *ui);
void ui_controller_update(UIController *ui, SynthState *state);

#ifdef __cplusplus
}
#endif

#endif // UI_CONTROLLER_H