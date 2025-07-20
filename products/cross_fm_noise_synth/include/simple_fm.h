#ifndef SIMPLE_FM_H
#define SIMPLE_FM_H

#include <cmath>
#include <algorithm>

/**
 * @brief シンプルなFMシンセサイザークラス
 * 
 * キャリアとモジュレーターの2オシレーターによる
 * 基本的なFM合成を実装
 */
class SimpleFM
{
public:
    void Init(float samplerate)
    {
        samplerate_ = samplerate;
        carrier_phase_ = 0.0f;
        modulator_phase_ = 0.0f;
        carrier_freq_ = 440.0f;
        modulator_freq_ = 220.0f;
        index_ = 1.0f;
    }

    void SetFrequency(float freq)
    {
        carrier_freq_ = freq;
    }

    void SetRatio(float ratio)
    {
        modulator_freq_ = carrier_freq_ * ratio;
    }

    void SetIndex(float index)
    {
        index_ = index;
    }

    float Process()
    {
        // モジュレーターの位相を更新
        modulator_phase_ += modulator_freq_ / samplerate_;
        if (modulator_phase_ >= 1.0f)
            modulator_phase_ -= 1.0f;

        // キャリアの位相を更新（モジュレーターの影響を受ける）
        float modulator = sinf(modulator_phase_ * 2.0f * M_PI) * index_;
        carrier_phase_ += (carrier_freq_ + modulator) / samplerate_;
        if (carrier_phase_ >= 1.0f)
            carrier_phase_ -= 1.0f;

        // キャリア波形を生成
        return sinf(carrier_phase_ * 2.0f * M_PI);
    }

private:
    float samplerate_;
    float carrier_phase_;
    float modulator_phase_;
    float carrier_freq_;
    float modulator_freq_;
    float index_;
};

#endif // SIMPLE_FM_H