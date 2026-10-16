#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
namespace gummy {
    constexpr float pi = 3.14159265358979323846f;
    inline float dbToGain(float x) noexcept {
        return std::pow(10.0f, x * .05f);
    }
    inline float gainToDb(float x) noexcept {
        return 20.0f * std::log10(std::max(x, 1.0e-10f));
    }
    inline float finite(float x) noexcept {
        return std::isfinite(x) ? std::clamp(x, -1.0e6f, 1.0e6f) : 0.0f;
    }
    inline float pole(double sr, float ms) noexcept {
        return std::exp(-1.0f / (float(sr) * .001f * std::max(ms, .01f)));
    }
    struct Slew {
        float value = 0, coefficient = 0;
        bool initial = true;
        void prepare(double sr, float ms = 20) noexcept {
            coefficient = pole(sr, ms);
            initial = true;
        }
        void reset(float x = 0) noexcept {
            value = x;
            initial = true;
        }
        float next(float target) noexcept {
            if (initial) {
                value = target;
                initial = false;
            }
            value = target + coefficient * (value - target);
            return value;
        }
    };
    struct Settings {
        float input = 0, output = 0, mix = 100;
        bool bypass = false;
        int quality = 1;
        bool transientOn = true;
        float attack = 0, sustain = 0;
        bool compOn = true;
        float threshold = 0, ratio = 1, compAttack = 10, release = 150, makeup = 0, compMix = 100;
        bool autoRelease = true;
        int compType = 0;
        bool colorOn = true;
        float colorDrive = 0, colorMix = 100;
        int colorType = 0;
        bool toneOn = true;
        float body = 0, presence = 0, air = 0;
        bool smoothOn = true;
        float smooth = 0;
        bool motionOn = true;
        float motionAmount = 0;
        int motionType = 0;
        bool clipOn = false;
        float clipDrive = 0, ceiling = 0;
        int clipMode = 0;
    };
    struct MeterValues {
        float input = 0, output = 0, reduction = 0, resonance = 0, clip = 0;
    };
} // namespace gummy
