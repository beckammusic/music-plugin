#pragma once
#include "Common.h"
namespace gummy {
    // Three simultaneous transfer curves: mode changes fade their outputs, never reset state.
    class Saturation {
        Slew drive, wet;
        std::array<Slew, 3> weights;
        std::array<float, 2> tapeLow{}, dcIn{}, dcOut{};
        float lowPole = 0, dcPole = 0;

      public:
        void prepare(double sr) {
            drive.prepare(sr);
            wet.prepare(sr);
            for (auto &w : weights)
                w.prepare(sr, 12);
            lowPole = std::exp(-2 * pi * 7500.0f / float(sr));
            dcPole = std::exp(-2 * pi * 12.0f / float(sr));
            reset();
        }
        void reset() {
            drive.reset();
            wet.reset();
            for (auto &w : weights)
                w.reset();
            tapeLow = {};
            dcIn = {};
            dcOut = {};
        }
        static float curve(float x, float gain, int type) noexcept {
            const float z = std::clamp(x * gain, -100.0f, 100.0f);
            if (type == 0)
                return std::tanh(z);
            if (type == 1) {
                constexpr float bias = .22f;
                return (std::tanh(z + bias) - std::tanh(bias)) / (1 - std::tanh(bias) * std::tanh(bias));
            }
            return .78f * std::tanh(z * 1.4f) + .22f * std::sin(std::clamp(z, -pi, pi));
        }
        void process(float &l, float &r, const Settings &s) noexcept {
            float amount = drive.next(std::clamp(s.colorDrive * .01f, 0.0f, 1.0f));
            float mix = wet.next(std::clamp(s.colorMix * .01f, 0.0f, 1.0f)) * amount;
            float gain = dbToGain(24 * amount), compensation = 1.0f / std::sqrt(gain);
            std::array<float, 3> w{};
            for (int k = 0; k < 3; ++k)
                w[size_t(k)] = weights[size_t(k)].next(k == s.colorType ? 1.0f : 0.0f);
            float *out[2]{&l, &r};
            for (size_t ch = 0; ch < 2; ++ch) {
                float x = *out[ch];
                tapeLow[ch] = lowPole * tapeLow[ch] + (1 - lowPole) * x;
                float tape = (.86f * curve(x, gain, 0) + .14f * curve(tapeLow[ch], gain, 0)) * compensation;
                float tube = curve(x, gain, 1) * compensation;
                float color = curve(x, gain, 2) * compensation * .85f;
                float y = w[0] * tape + w[1] * tube + w[2] * color;
                float blocked = y - dcIn[ch] + dcPole * dcOut[ch];
                dcIn[ch] = y;
                dcOut[ch] = blocked;
                *out[ch] = finite(x + mix * (blocked - x));
            }
        }
    };
} // namespace gummy
