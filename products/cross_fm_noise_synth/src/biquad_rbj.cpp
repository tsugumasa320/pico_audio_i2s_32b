#include "../include/biquad_rbj.h"

using namespace daisysp;

void BiquadRBJ::Init(float sample_rate)
{
    sample_rate_ = sample_rate;
    cutoff_      = 1000.0f; // デフォルトのカットオフ周波数
    q_           = 0.707f;  // デフォルトのQ値（1/sqrt(2)）
    gain_        = 0.0f;    // デフォルトのゲイン（dB）
    type_        = LOWPASS; // デフォルトのフィルタタイプ
    xnm1_ = xnm2_ = ynm1_ = ynm2_ = 0.0f;
    UpdateCoefficients();
}

void BiquadRBJ::SetType(BiquadType type)
{
    type_ = type;
    UpdateCoefficients();
}

void BiquadRBJ::SetCutoff(float cutoff)
{
    cutoff_ = cutoff;
    UpdateCoefficients();
}

void BiquadRBJ::SetQ(float q)
{
    q_ = q;
    UpdateCoefficients();
}

void BiquadRBJ::SetGain(float gain)
{
    gain_ = gain;
    UpdateCoefficients();
}

void BiquadRBJ::UpdateCoefficients()
{
    float omega = 2.0f * M_PI * cutoff_ / sample_rate_;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * q_);
    float A = powf(10.0f, gain_ / 40.0f); // ゲイン（dB）を線形値に変換

    switch (type_)
    {
        case LOWPASS:
            b0_ = (1.0f - cos_omega) / 2.0f;
            b1_ = 1.0f - cos_omega;
            b2_ = (1.0f - cos_omega) / 2.0f;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_omega;
            a2_ = 1.0f - alpha;
            break;

        case HIGHPASS:
            b0_ = (1.0f + cos_omega) / 2.0f;
            b1_ = -(1.0f + cos_omega);
            b2_ = (1.0f + cos_omega) / 2.0f;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_omega;
            a2_ = 1.0f - alpha;
            break;

        case BANDPASS:
            // constant 0 dB peak gain
            b0_ = alpha;
            b1_ = 0.0f;
            b2_ = -alpha;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_omega;
            a2_ = 1.0f - alpha;
            break;

        case NOTCH:
            b0_ = 1.0f;
            b1_ = -2.0f * cos_omega;
            b2_ = 1.0f;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cos_omega;
            a2_ = 1.0f - alpha;
            break;

        case PEAK:
            b0_ = 1.0f + alpha * A;
            b1_ = -2.0f * cos_omega;
            b2_ = 1.0f - alpha * A;
            a0_ = 1.0f + alpha / A;
            a1_ = -2.0f * cos_omega;
            a2_ = 1.0f - alpha / A;
            break;

        case LOWSHELF:
            b0_ = A * ((A + 1.0f) - (A - 1.0f) * cos_omega + 2.0f * sqrtf(A) * alpha);
            b1_ = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_omega);
            b2_ = A * ((A + 1.0f) - (A - 1.0f) * cos_omega - 2.0f * sqrtf(A) * alpha);
            a0_ = (A + 1.0f) + (A - 1.0f) * cos_omega + 2.0f * sqrtf(A) * alpha;
            a1_ = -2.0f * ((A - 1.0f) + (A + 1.0f) * cos_omega);
            a2_ = (A + 1.0f) + (A - 1.0f) * cos_omega - 2.0f * sqrtf(A) * alpha;
            break;

        case HIGHSHELF:
            b0_ = A * ((A + 1.0f) + (A - 1.0f) * cos_omega + 2.0f * sqrtf(A) * alpha);
            b1_ = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_omega);
            b2_ = A * ((A + 1.0f) + (A - 1.0f) * cos_omega - 2.0f * sqrtf(A) * alpha);
            a0_ = (A + 1.0f) - (A - 1.0f) * cos_omega + 2.0f * sqrtf(A) * alpha;
            a1_ = 2.0f * ((A - 1.0f) - (A + 1.0f) * cos_omega);
            a2_ = (A + 1.0f) - (A - 1.0f) * cos_omega - 2.0f * sqrtf(A) * alpha;
            break;
    }

    // Normalize coefficients
    // a0_ is always 1.0f, so we can divide all coefficients by a0_
    b0_ /= a0_;
    b1_ /= a0_;
    b2_ /= a0_;
    a1_ /= a0_;
    a2_ /= a0_;
    a0_ = 1.0f;
}

float BiquadRBJ::Process(float in)
{
    float out = b0_ * in + b1_ * xnm1_ + b2_ * xnm2_ - a1_ * ynm1_ - a2_ * ynm2_;
    xnm2_ = xnm1_;
    xnm1_ = in;
    ynm2_ = ynm1_;
    ynm1_ = out;
    return out;
}