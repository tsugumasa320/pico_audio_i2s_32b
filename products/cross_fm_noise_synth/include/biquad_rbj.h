#pragma once
#ifndef RBJ_BIQUAD_H
#define RBJ_BIQUAD_H

#include <stdint.h>
#include <math.h>

#ifdef __cplusplus

namespace daisysp
{

/** Biquadフィルタの種類 */
enum BiquadType
{
    LOWPASS,
    HIGHPASS,
    BANDPASS,
    NOTCH,
    PEAK,
    LOWSHELF,
    HIGHSHELF
};

/** 汎用的なBiquadフィルタクラス */
class BiquadRBJ
{
  public:
    BiquadRBJ() {}
    ~BiquadRBJ() {}

    /** 初期化
        \param sample_rate - サンプルレート
    */
    void Init(float sample_rate);

    /** 入力信号をフィルタリング
        \param in - 入力信号
        \return フィルタリング後の信号
    */
    float Process(float in);

    /** フィルタの種類を設定
        \param type - フィルタの種類（LOWPASS, HIGHPASSなど）
    */
    void SetType(BiquadType type);

    /** カットオフ周波数を設定
        \param cutoff - カットオフ周波数（Hz）
    */
    void SetCutoff(float cutoff);

    /** Q値（共振）を設定
        \param q - Q値
    */
    void SetQ(float q);

    /** ゲインを設定（シェルビングフィルタ用）
        \param gain - ゲイン（dB）
    */
    void SetGain(float gain);

  private:
    void UpdateCoefficients();

    float sample_rate_, cutoff_, q_, gain_;
    BiquadType type_;
    float a0_, a1_, a2_, b0_, b1_, b2_;
    float xnm1_, xnm2_, ynm1_, ynm2_;
};

} // namespace daisysp

#endif
#endif