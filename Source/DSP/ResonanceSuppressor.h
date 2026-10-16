#pragma once
#include "Common.h"

namespace gummy {

// A zero-lookahead adaptive damping bank.  Both channels use the same band
// attenuation, so a resonance on one side cannot move the stereo image.
class ResonanceSuppressor {
public:
    ResonanceSuppressor() noexcept { prepare(44100.0); }

    void prepare(double sampleRate) noexcept {
        sampleRate_ = std::isfinite(sampleRate) ? std::clamp(sampleRate, 8000.0, 768000.0) : 44100.0;
        const double top = std::min(16000.0, sampleRate_ * 0.42);
        for (std::size_t i = 0; i < bandCount; ++i) {
            auto& band = bands_[i];
            const double frequency = 160.0 * std::pow(top / 160.0, double(i) / double(bandCount - 1));
            const double omega = 2.0 * double(pi) * frequency / sampleRate_;
            const double alpha = std::sin(omega) / (2.0 * 7.0);
            band.left.b0 = band.right.b0 = float(alpha / (1.0 + alpha));
            band.left.a1 = band.right.a1 = float(-2.0 * std::cos(omega) / (1.0 + alpha));
            band.left.a2 = band.right.a2 = float((1.0 - alpha) / (1.0 + alpha));
            band.frequency = float(frequency);
            // A partial bandwidth correction keeps both pink and white spectra
            // sufficiently flat for the musical-content confidence detector.
            band.densityWeight = std::sqrt(1000.0f / band.frequency);
        }
        envelopeAttack_ = pole(sampleRate_, 8.0f);
        envelopeRelease_ = pole(sampleRate_, 100.0f);
        cutAttack_ = pole(sampleRate_, 20.0f);
        cutRelease_ = pole(sampleRate_, 180.0f);
        amount_.prepare(sampleRate_, 35.0f);
        reset();
    }

    void reset() noexcept {
        for (auto& band : bands_) {
            band.left.reset();
            band.right.reset();
            band.power = band.target = band.cut = 0.0f;
        }
        amount_.reset();
        untilAnalysis_ = 0;
        activity_ = 0.0f;
    }

    void process(float& left, float& right, const Settings& settings) noexcept {
        const float inputL = finite(left), inputR = finite(right);
        const float requested = std::clamp(finite(settings.smooth), 0.0f, 100.0f) * 0.01f;
        const float amount = amount_.next(requested);
        std::array<float, bandCount> bandL{}, bandR{};

        for (std::size_t i = 0; i < bandCount; ++i) {
            auto& band = bands_[i];
            bandL[i] = band.left.process(inputL);
            bandR[i] = band.right.process(inputR);
            const float power = 0.5f * (bandL[i] * bandL[i] + bandR[i] * bandR[i]);
            const float coefficient = power > band.power ? envelopeAttack_ : envelopeRelease_;
            band.power = power + coefficient * (band.power - power);
            if (band.power < 1.0e-20f) band.power = 0.0f;
        }

        if (requested == 0.0f) {
            // Release existing cuts without a discontinuity during automation.
            // Fresh/settled zero remains exactly dry and analysis stays warm.
            for (auto& band : bands_) band.target = 0.0f;
            untilAnalysis_ = 0;
        } else if (--untilAnalysis_ <= 0) {
            updateTargets(amount);
            untilAnalysis_ = analysisInterval;
        }

        float removedL = 0.0f, removedR = 0.0f, strongest = 0.0f;
        for (std::size_t i = 0; i < bandCount; ++i) {
            auto& band = bands_[i];
            const float coefficient = requested == 0.0f || band.target > band.cut ? cutAttack_ : cutRelease_;
            band.cut = band.target + coefficient * (band.cut - band.target);
            if (band.cut < 1.0e-10f) band.cut = 0.0f;
            removedL += band.cut * bandL[i];
            removedR += band.cut * bandR[i];
            strongest = std::max(strongest, band.cut);
        }
        left = finite(inputL - removedL);
        right = finite(inputR - removedR);
        activity_ = -gainToDb(std::max(1.0f - strongest, 0.1f));
    }

    // Positive nominal attenuation of the most active band, in dB. This is a
    // band-activity meter, rather than a broadband input/output level ratio.
    float activity() const noexcept { return activity_; }

private:
    static constexpr std::size_t bandCount = 24;
    static constexpr int analysisInterval = 32;

    struct Bandpass {
        float b0 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
        void reset() noexcept { z1 = z2 = 0.0f; }
        float process(float input) noexcept {
            const float output = b0 * input + z1;
            z1 = -a1 * output + z2;
            z2 = -b0 * input - a2 * output;
            if (std::abs(z1) < 1.0e-20f) z1 = 0.0f;
            if (std::abs(z2) < 1.0e-20f) z2 = 0.0f;
            return output;
        }
    };
    struct Band {
        Bandpass left, right;
        float frequency = 1000.0f, densityWeight = 1.0f;
        float power = 0.0f, target = 0.0f, cut = 0.0f;
    };

    void updateTargets(float amount) noexcept {
        float arithmetic = 0.0f, logarithmic = 0.0f;
        for (const auto& band : bands_) {
            const float density = std::max(band.power * band.densityWeight, 1.0e-18f);
            arithmetic += density;
            logarithmic += std::log(density);
        }
        arithmetic /= float(bandCount);
        const float flatness = std::exp(logarithmic / float(bandCount)) / std::max(arithmetic, 1.0e-18f);
        // Isolated steady tones have very low flatness. Reducing confidence for
        // those spectra prevents every intentional tonal peak becoming a notch.
        const float confidence = std::clamp((flatness - 0.12f) / 0.12f, 0.0f, 1.0f);
        bands_.front().target = bands_.back().target = 0.0f;
        for (std::size_t i = 1; i + 1 < bandCount; ++i) {
            auto& band = bands_[i];
            // The next-but-one neighbors cover peaks falling between two bank
            // centers, while the geometric mean compensates spectral tilt.
            const std::size_t below = i >= 2 ? i - 2 : 0;
            const std::size_t above = std::min(i + 2, bandCount - 1);
            const float neighbors = std::sqrt(std::max(bands_[below].power, 1.0e-18f)
                                            * std::max(bands_[above].power, 1.0e-18f));
            const float prominence = 10.0f * std::log10(std::max(band.power, 1.0e-18f)
                                                        / std::max(neighbors, 1.0e-18f));
            const float emphasis = std::clamp((band.frequency - 220.0f) / 650.0f, 0.0f, 1.0f);
            const float threshold = 11.0f - 6.5f * amount + 4.0f * (1.0f - emphasis);
            const float audible = std::clamp((10.0f * std::log10(std::max(band.power, 1.0e-18f))
                                             + 70.0f) / 15.0f, 0.0f, 1.0f);
            const float depth = std::min(9.0f * amount,
                                         std::max(0.0f, prominence - threshold) * 1.6f)
                                * emphasis * confidence * audible;
            band.target = 1.0f - dbToGain(-depth);
        }
    }

    std::array<Band, bandCount> bands_{};
    Slew amount_;
    double sampleRate_ = 44100.0;
    float envelopeAttack_ = 0.0f, envelopeRelease_ = 0.0f;
    float cutAttack_ = 0.0f, cutRelease_ = 0.0f, activity_ = 0.0f;
    int untilAnalysis_ = 0;
};

} // namespace gummy
