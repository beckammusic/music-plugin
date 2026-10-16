#pragma once
#include "Common.h"

namespace gummy {

// Linked, zero-latency envelope shaper. Envelopes keep running at neutral settings.
class TransientShaper {
public:
    void prepare(double sampleRate) noexcept {
        sampleRate_ = std::isfinite(sampleRate) && sampleRate > 1000.0 ? sampleRate : 48000.0;
        fastAttack_ = pole(sampleRate_, 0.4f);
        fastRelease_ = pole(sampleRate_, 8.0f);
        slowAttack_ = pole(sampleRate_, 20.0f);
        slowRelease_ = pole(sampleRate_, 120.0f);
        attackRise_ = pole(sampleRate_, 0.25f);
        attackFall_ = pole(sampleRate_, 3.0f);
        sustainPole_ = pole(sampleRate_, 3.0f);
        attack_.prepare(sampleRate_);
        sustain_.prepare(sampleRate_);
        reset();
    }

    void reset() noexcept {
        fast_ = slow_ = attackControl_ = sustainControl_ = 0.0f;
        attack_.reset();
        sustain_.reset();
    }

    void process(float& left, float& right, const Settings& settings) noexcept {
        left = finite(left);
        right = finite(right);
        const float attack = attack_.next(std::clamp(finite(settings.attack), -100.0f, 100.0f) * .01f);
        const float sustain = sustain_.next(std::clamp(finite(settings.sustain), -100.0f, 100.0f) * .01f);
        const float level = std::max(std::abs(left), std::abs(right));
        follow(fast_, level, fastAttack_, fastRelease_);
        follow(slow_, level, slowAttack_, slowRelease_);

        const float onset = std::clamp((fast_ - slow_) / std::max(fast_, 1.0e-10f), 0.0f, 1.0f);
        // Fade the detector's action below -80 dBFS instead of lifting background noise.
        const float activity = std::clamp(slow_ * 10000.0f, 0.0f, 1.0f);
        const float attackTarget = onset * activity;
        const float sustainTarget = (1.0f - onset) * activity;
        follow(attackControl_, attackTarget, attackRise_, attackFall_);
        follow(sustainControl_, sustainTarget, sustainPole_, sustainPole_);

        // This explicit branch guarantees sample-exact unity at initialized neutral.
        if (attack == 0.0f && sustain == 0.0f)
            return;
        const float gain = dbToGain(12.0f * attack * attackControl_ + 9.0f * sustain * sustainControl_);
        left *= gain;
        right *= gain;
    }

private:
    static void follow(float& envelope, float target, float attack, float release) noexcept {
        const float coefficient = target > envelope ? attack : release;
        envelope = target + coefficient * (envelope - target);
        if (envelope < 1.0e-20f)
            envelope = 0.0f;
    }
    double sampleRate_ = 48000.0;
    float fastAttack_ = .95f, fastRelease_ = .997f, slowAttack_ = .999f, slowRelease_ = .9998f;
    float attackRise_ = .92f, attackFall_ = .993f, sustainPole_ = .993f;
    float fast_ = 0, slow_ = 0, attackControl_ = 0, sustainControl_ = 0;
    Slew attack_, sustain_;
};

} // namespace gummy
