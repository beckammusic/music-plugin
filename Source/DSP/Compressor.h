#pragma once
#include "Common.h"

namespace gummy {

// Feed-forward stereo compressor. All three characters remain warm for mode fades.
class Compressor {
public:
    void prepare(double sampleRate) noexcept {
        sampleRate_ = std::isfinite(sampleRate) && sampleRate > 1000.0 ? sampleRate : 48000.0;
        rmsSmoothPole_ = pole(sampleRate_, 10.0f);
        rmsGluePole_ = pole(sampleRate_, 5.0f);
        peakAttackPole_ = pole(sampleRate_, .5f);
        peakReleasePole_ = pole(sampleRate_, 30.0f);
        programAttackPole_ = pole(sampleRate_, 50.0f);
        programReleasePole_ = pole(sampleRate_, 1000.0f);
        for (auto* smoother : { &threshold_, &ratio_, &attack_, &release_, &makeup_, &mix_, &auto_ })
            smoother->prepare(sampleRate_, 20.0f);
        for (auto& weight : modeWeights_)
            weight.prepare(sampleRate_, 10.0f);
        reset();
    }

    void reset() noexcept {
        smoothEnergy_ = glueEnergy_ = peak_ = reduction_ = 0.0f;
        states_ = {};
        for (auto* smoother : { &threshold_, &ratio_, &attack_, &release_, &makeup_, &mix_, &auto_ })
            smoother->reset();
        for (auto& weight : modeWeights_)
            weight.reset();
    }

    // Positive dB of linked, pre-makeup gain reduction. Safe for audio-thread reads.
    float reduction() const noexcept { return reduction_; }

    void process(float& left, float& right, const Settings& settings) noexcept {
        left = finite(left);
        right = finite(right);
        const float threshold = threshold_.next(std::clamp(finite(settings.threshold), -60.0f, 0.0f));
        const float ratio = ratio_.next(std::clamp(finite(settings.ratio), 1.0f, 20.0f));
        const float attack = attack_.next(std::clamp(finite(settings.compAttack), .1f, 100.0f));
        const float release = release_.next(std::clamp(finite(settings.release), 10.0f, 2000.0f));
        const float makeup = makeup_.next(std::clamp(finite(settings.makeup), -12.0f, 24.0f));
        const float mix = mix_.next(std::clamp(finite(settings.compMix), 0.0f, 100.0f) * .01f);
        const float automatic = auto_.next(settings.autoRelease ? 1.0f : 0.0f);
        const int type = std::clamp(settings.compType, 0, 2);

        const float peakLevel = std::max(std::abs(left), std::abs(right));
        const float energy = peakLevel * peakLevel;
        follow(smoothEnergy_, energy, rmsSmoothPole_, rmsSmoothPole_);
        follow(glueEnergy_, energy, rmsGluePole_, rmsGluePole_);
        follow(peak_, peakLevel, peakAttackPole_, peakReleasePole_);
        const float smoothLevel = std::sqrt(smoothEnergy_);
        const float glueRms = std::sqrt(glueEnergy_);
        const std::array<float, 3> levels {{smoothLevel, peakLevel, .65f * glueRms + .35f * peak_}};
        const float crest = std::clamp((peakLevel - glueRms) / std::max(peakLevel, 1.0e-10f), 0.0f, 1.0f);
        constexpr std::array<float, 3> knees {{6.0f, 1.0f, 4.0f}};
        constexpr std::array<float, 3> releaseScales {{1.0f, .7f, 1.15f}};

        reduction_ = 0.0f;
        for (std::size_t mode = 0; mode < states_.size(); ++mode) {
            auto& state = states_[mode];
            const float target = gainReduction(gainToDb(levels[mode]), threshold, ratio, knees[mode]);
            follow(state.program, target, programAttackPole_, programReleasePole_);
            // Brief transients recover quickly; sustained reduction gradually lengthens recovery.
            const float programScale = .55f + 1.45f * std::clamp(state.program / 24.0f, 0.0f, 1.0f);
            const float releaseMs = release * releaseScales[mode] * (1.0f + automatic * (programScale - 1.0f));
            // Punch's crest-dependent attack lets the leading edge through before clamping the body.
            const float attackMs = attack * (mode == 1 ? (1.0f + 1.5f * crest) : 1.0f);
            follow(state.gain, target, pole(sampleRate_, attackMs), pole(sampleRate_, releaseMs));
            reduction_ += modeWeights_[mode].next(static_cast<int>(mode) == type ? 1.0f : 0.0f) * state.gain;
        }
        const float gain = dbToGain(makeup - reduction_);
        left += mix * (left * gain - left);
        right += mix * (right * gain - right);
    }

private:
    struct ModeState { float gain = 0.0f, program = 0.0f; };
    static float gainReduction(float levelDb, float threshold, float ratio, float knee) noexcept {
        const float difference = levelDb - threshold;
        const float slope = 1.0f - 1.0f / ratio;
        if (difference <= -.5f * knee)
            return 0.0f;
        if (difference >= .5f * knee)
            return slope * difference;
        const float position = difference + .5f * knee;
        return slope * position * position / (2.0f * knee);
    }
    static void follow(float& value, float target, float attack, float release) noexcept {
        const float coefficient = target > value ? attack : release;
        value = target + coefficient * (value - target);
        if (value < 1.0e-20f)
            value = 0.0f;
    }
    double sampleRate_ = 48000.0;
    Slew threshold_, ratio_, attack_, release_, makeup_, mix_, auto_;
    std::array<Slew, 3> modeWeights_;
    std::array<ModeState, 3> states_ {};
    float smoothEnergy_ = 0, glueEnergy_ = 0, peak_ = 0, reduction_ = 0;
    float rmsSmoothPole_ = .998f, rmsGluePole_ = .996f, peakAttackPole_ = .96f, peakReleasePole_ = .999f;
    float programAttackPole_ = .9996f, programReleasePole_ = .99998f;
};

} // namespace gummy
