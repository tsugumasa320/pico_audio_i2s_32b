#ifndef CROSS_MOD_H
#define CROSS_MOD_H

#include <cmath>

/**
 * @brief クロスモジュレーションクラス
 * 
 * FMとノイズ信号間の相互変調を実装
 */
class CrossMod
{
public:
    void Init(float samplerate)
    {
        samplerate_ = samplerate;
        lfo_phase_ = 0.0f;
        lfo_freq_ = 0.5f;  // 0.5Hz LFO
        depth_ = 0.0f;
        rate_ = 1.0f;
    }

    void SetDepth(float depth)
    {
        depth_ = depth;
    }

    void SetRate(float rate)
    {
        lfo_freq_ = rate;
    }

    /**
     * @brief クロスモジュレーション処理
     * @param fm_signal FM信号
     * @param noise_signal ノイズ信号
     * @return モジュレーション結果
     */
    float Process(float fm_signal, float noise_signal)
    {
        // LFOの位相を更新
        lfo_phase_ += lfo_freq_ / samplerate_;
        if (lfo_phase_ >= 1.0f)
            lfo_phase_ -= 1.0f;

        // LFO波形（サイン波）
        float lfo = sinf(lfo_phase_ * 2.0f * M_PI);

        // クロスモジュレーション
        // FM信号でノイズを変調し、ノイズでFM信号を変調
        float fm_mod_noise = noise_signal * (1.0f + fm_signal * depth_ * lfo);
        float noise_mod_fm = fm_signal * (1.0f + noise_signal * depth_ * lfo);

        // 結果をミックス
        return (fm_mod_noise + noise_mod_fm) * 0.5f;
    }

private:
    float samplerate_;
    float lfo_phase_;
    float lfo_freq_;
    float depth_;
    float rate_;
};

#endif // CROSS_MOD_H