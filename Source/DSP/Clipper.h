#pragma once
#include "Common.h"
namespace gummy {
    class Clipper {
        Slew drive, limit, soft;
        float reduction_ = 0;

      public:
        void prepare(double sr) {
            drive.prepare(sr);
            limit.prepare(sr);
            soft.prepare(sr, 12);
            reset();
        }
        void reset() {
            drive.reset();
            limit.reset();
            soft.reset();
            reduction_ = 0;
        }
        static float hard(float x) noexcept { return std::clamp(x, -1.0f, 1.0f); }
        static float gentle(float x) noexcept {
            float a = std::abs(x);
            return a <= .8f ? x : std::copysign(.8f + .2f * std::tanh((a - .8f) * 5.0f), x);
        }
        void process(float &l, float &r, const Settings &s) noexcept {
            float gain = dbToGain(drive.next(s.clipDrive));
            float ceiling = dbToGain(limit.next(s.ceiling));
            float blend = soft.next(s.clipMode == 1 ? 1.0f : 0.0f);
            float inPeak = std::max(std::abs(l), std::abs(r)) * gain;
            for (float *sample : {&l, &r}) {
                float x = *sample * gain / ceiling;
                *sample = ceiling * (hard(x) + blend * (gentle(x) - hard(x)));
            }
            reduction_ =
                std::max(0.0f, gainToDb(inPeak / std::max(std::max(std::abs(l), std::abs(r)), 1e-10f)));
        }
        float activity() const noexcept { return reduction_; }
    };
} // namespace gummy
