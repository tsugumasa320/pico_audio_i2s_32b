#ifndef SIMPLE_NOISE_H
#define SIMPLE_NOISE_H

#include <cstdint>

/**
 * @brief シンプルなノイズジェネレータークラス
 * 
 * ホワイトノイズとピンクノイズの生成を実装
 */
class SimpleNoise
{
public:
    enum NoiseType {
        WHITE_NOISE = 0,
        PINK_NOISE = 1
    };

    void Init()
    {
        seed_ = 1;
        pink_state_[0] = 0.0f;
        pink_state_[1] = 0.0f;
        pink_state_[2] = 0.0f;
        noise_type_ = WHITE_NOISE;
        level_ = 0.5f;
    }

    void SetType(NoiseType type)
    {
        noise_type_ = type;
    }

    void SetLevel(float level)
    {
        level_ = level;
    }

    float Process()
    {
        float output = 0.0f;
        
        switch (noise_type_) {
            case WHITE_NOISE:
                output = GenerateWhiteNoise();
                break;
            case PINK_NOISE:
                output = GeneratePinkNoise();
                break;
        }
        
        return output * level_;
    }

private:
    uint32_t seed_;
    float pink_state_[3];
    NoiseType noise_type_;
    float level_;

    // 線形合同法による擬似乱数生成
    float GenerateWhiteNoise()
    {
        seed_ = seed_ * 1103515245 + 12345;
        return ((float)(seed_ & 0x7FFFFFFF) / (float)0x7FFFFFFF) * 2.0f - 1.0f;
    }

    // シンプルなピンクノイズフィルター
    float GeneratePinkNoise()
    {
        float white = GenerateWhiteNoise();
        
        // 3段のローパスフィルターでピンクノイズ近似
        pink_state_[0] = 0.99886f * pink_state_[0] + white * 0.0555179f;
        pink_state_[1] = 0.99332f * pink_state_[1] + white * 0.0750759f;
        pink_state_[2] = 0.96900f * pink_state_[2] + white * 0.1538520f;
        
        float pink = pink_state_[0] + pink_state_[1] + pink_state_[2] + white * 0.3104856f;
        return pink * 0.11f; // レベル調整
    }
};

#endif // SIMPLE_NOISE_H