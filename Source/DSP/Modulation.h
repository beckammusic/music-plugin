#pragma once
#include "Common.h"

namespace gummy {

// Three continuously running voices make selector automation independent of
// delay-line history. Only prepare() allocates memory.
class Modulation {
public:
    void prepare(double sampleRate) {
        sr = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 768000.0) : 48000.0;
        const auto size = static_cast<std::size_t>(std::ceil(sr * .065)) + 8;
        for (auto& line : lines) line.prepare(size);
        amount.prepare(sr, 35.0f);
        modePole = pole(sr, 30.0f);
        reset();
    }

    void reset() noexcept {
        for (auto& line : lines) line.reset();
        phases = {0.0, 1.17, 2.31, .61};
        weights = {1.0f, 0.0f, 0.0f};
        amount.reset();
        firstMode = true;
    }

    void process(float& left, float& right, const Settings& settings) noexcept {
        const float dryL = finite(left), dryR = finite(right);
        if (lines[0].left.empty()) { left = dryL; right = dryR; return; }
        const float a = amount.next(std::clamp(finite(settings.motionAmount), 0.0f, 100.0f) * .01f);
        const int mode = std::clamp(settings.motionType, 0, 2);
        if (firstMode) {
            weights = {0, 0, 0}; weights[static_cast<std::size_t>(mode)] = 1;
            firstMode = false;
        }
        for (std::size_t i = 0; i < weights.size(); ++i) {
            const float target = static_cast<int>(i) == mode ? 1.0f : 0.0f;
            weights[i] = target + modePole * (weights[i] - target);
        }

        advance(0, .18 + .22 * a); advance(1, .09 + .11 * a);
        advance(2, .41); advance(3, .67);
        const auto chorus = chorusTick(dryL, dryR, a);
        const auto flanger = flangerTick(dryL, dryR, a);
        const auto doubler = doublerTick(dryL, dryR, a);
        // Normalization also removes very small accumulated floating-point drift
        // during long automation runs. The weights never overshoot.
        const float sum = weights[0] + weights[1] + weights[2];
        left = finite((chorus[0] * weights[0] + flanger[0] * weights[1] + doubler[0] * weights[2]) / sum);
        right = finite((chorus[1] * weights[0] + flanger[1] * weights[1] + doubler[1] * weights[2]) / sum);
        if (a == 0) { left = dryL; right = dryR; }
    }

private:
    struct Delay {
        std::vector<float> left, right;
        std::size_t writeIndex = 0;
        void prepare(std::size_t size) { left.assign(size, 0.0f); right.assign(size, 0.0f); writeIndex = 0; }
        void reset() noexcept {
            std::fill(left.begin(), left.end(), 0.0f);
            std::fill(right.begin(), right.end(), 0.0f);
            writeIndex = 0;
        }
        float read(double delay, bool r) const noexcept {
            const auto& buffer = r ? right : left;
            const auto size = static_cast<int>(buffer.size());
            delay = std::clamp(delay, 3.0, double(size - 4));
            double position = double(writeIndex) - delay;
            if (position < 0) position += double(size);
            const auto index = static_cast<int>(position);
            const float t = static_cast<float>(position - double(index));
            auto at = [&](int i) noexcept {
                if (i < 0) i += size;
                if (i >= size) i -= size;
                return buffer[static_cast<std::size_t>(i)];
            };
            // Four-point cubic Hermite interpolation reduces fractional-delay
            // treble loss. Its bounded overshoot is included in feedback margin.
            const float p0 = at(index - 1), p1 = at(index), p2 = at(index + 1), p3 = at(index + 2);
            const float c1 = .5f * (p2 - p0);
            const float c2 = p0 - 2.5f * p1 + 2.0f * p2 - .5f * p3;
            const float c3 = .5f * (p3 - p0) + 1.5f * (p1 - p2);
            return ((c3 * t + c2) * t + c1) * t + p1;
        }
        void write(float l, float r) noexcept {
            left[writeIndex] = l; right[writeIndex] = r;
            if (++writeIndex >= left.size()) writeIndex = 0;
        }
    };

    void advance(std::size_t index, double hz) noexcept {
        phases[index] += 2.0 * double(pi) * hz / sr;
        if (phases[index] >= 2.0 * double(pi)) phases[index] -= 2.0 * double(pi);
    }
    std::array<float, 2> chorusTick(float l, float r, float a) noexcept {
        auto& line = lines[0];
        const double depth = .001 + .0025 * a;
        const float wl = line.read(sr * (.013 + depth * std::sin(phases[0])), false);
        const float wr = line.read(sr * (.014 + depth * std::sin(phases[0] + 1.1)), true);
        line.write(l, r);
        const float mix = .42f * a;
        return {l * (1 - mix) + mix * (.9f * wl + .1f * wr),
                r * (1 - mix) + mix * (.9f * wr + .1f * wl)};
    }
    std::array<float, 2> flangerTick(float l, float r, float a) noexcept {
        auto& line = lines[1];
        const double depth = .001 + .0038 * a;
        const float wl = line.read(sr * (.0007 + depth * (.5 + .5 * std::sin(phases[1]))), false);
        const float wr = line.read(sr * (.0007 + depth * (.5 + .5 * std::sin(phases[1] + .65))), true);
        const float feedback = .58f * a;
        line.write(finite(l + feedback * 8.0f * std::tanh(wl * .125f)),
                   finite(r + feedback * 8.0f * std::tanh(wr * .125f)));
        const float mix = .45f * a;
        return {l * (1 - mix) + mix * wl, r * (1 - mix) + mix * wr};
    }
    std::array<float, 2> doublerTick(float l, float r, float a) noexcept {
        auto& line = lines[2];
        const float wl = .65f * line.read(sr * (.019 + .0025 * a * std::sin(phases[2])), false)
                       + .35f * line.read(sr * (.030 + .0014 * a * std::sin(phases[3] + .7)), false);
        const float wr = .65f * line.read(sr * (.024 + .0025 * a * std::sin(phases[3])), true)
                       + .35f * line.read(sr * (.027 + .0018 * a * std::sin(phases[2] + 1.6)), true);
        line.write(l, r);
        const float mix = .40f * a;
        return {l * (1 - mix) + mix * wl, r * (1 - mix) + mix * wr};
    }

    double sr = 48000.0;
    std::array<Delay, 3> lines;
    std::array<double, 4> phases{};
    std::array<float, 3> weights{1, 0, 0};
    Slew amount;
    float modePole = 0;
    bool firstMode = true;
};
}
