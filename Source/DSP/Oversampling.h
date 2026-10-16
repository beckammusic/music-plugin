#pragma once
#include "Clipper.h"
#include "Saturation.h"
#include <juce_dsp/juce_dsp.h>
namespace gummy {
    // Small fixed delays cover JUCE's integer-latency linear-phase FIR stages.
    struct FixedDelay {
        std::array<std::array<float, 2048>, 2> memory{};
        size_t pos = 0;
        int delay = 0;
        void reset(int n) {
            delay = n;
            pos = 0;
            for (auto &c : memory)
                c.fill(0);
        }
        void process(float &l, float &r) noexcept {
            if (delay == 0)
                return;
            auto index = (pos + 2048 - size_t(delay)) % 2048;
            float a = memory[0][index], b = memory[1][index];
            memory[0][pos] = l;
            memory[1][pos] = r;
            l = a;
            r = b;
            pos = (pos + 1) % 2048;
        }
    };
    class NonlinearStage {
        struct Bank {
            std::unique_ptr<juce::dsp::Oversampling<float>> os;
            juce::AudioBuffer<float> buffer;
            Saturation saturation;
            Clipper clip;
            FixedDelay pad, highPad;
            int latency = 0;
        };
        std::array<Bank, 3> banks;
        std::array<Slew, 3> quality;
        Slew enabled, finalCeiling;
        FixedDelay dry;
        bool isClip = false;
        int latency_ = 0;

      public:
        void prepare(double sr, int capacity, bool clipper) {
            isClip = clipper;
            latency_ = 0;
            for (int k = 0; k < 3; ++k) {
                auto &b = banks[size_t(k)];
                b.os = std::make_unique<juce::dsp::Oversampling<float>>(
                    2, size_t(k), juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
                b.os->initProcessing(size_t(capacity));
                b.buffer.setSize(2, capacity);
                b.saturation.prepare(sr * (1 << k));
                b.clip.prepare(sr * (1 << k));
                // Finish fractional FIR delay at the oversampled rate, preserving linear phase.
                float rawLatency = b.os->getLatencyInSamples();
                b.latency = int(std::ceil(rawLatency - 1.0e-5f));
                b.highPad.reset(int(std::lround((float(b.latency) - rawLatency) * float(1 << k))));
                latency_ = std::max(latency_, b.latency);
                quality[size_t(k)].prepare(sr, 20);
            }
            jassert(latency_ < 1024);
            for (auto &b : banks)
                b.pad.reset(latency_ - b.latency);
            dry.reset(latency_);
            enabled.prepare(sr, 8);
            finalCeiling.prepare(sr, 20);
        }
        int latency() const noexcept { return latency_; }
        float process(juce::AudioBuffer<float> &buffer, int n, const Settings &settings) noexcept {
            float activity = 0;
            std::array<float, 3> bankActivity{};
            size_t bankIndex = 0;
            for (auto &b : banks) {
                for (int c = 0; c < 2; ++c)
                    b.buffer.copyFrom(c, 0, buffer, c, 0, n);
                juce::dsp::AudioBlock<float> base(b.buffer);
                auto sub = base.getSubBlock(0, size_t(n));
                auto up = b.os->processSamplesUp(sub);
                for (size_t i = 0; i < up.getNumSamples(); ++i) {
                    float &l = up.getChannelPointer(0)[i];
                    float &r = up.getChannelPointer(1)[i];
                    if (isClip) {
                        b.clip.process(l, r, settings);
                        bankActivity[bankIndex] = std::max(bankActivity[bankIndex], b.clip.activity());
                    } else
                        b.saturation.process(l, r, settings);
                    b.highPad.process(l, r);
                }
                b.os->processSamplesDown(sub);
                for (int i = 0; i < n; ++i) {
                    float &l = b.buffer.getWritePointer(0)[i];
                    float &r = b.buffer.getWritePointer(1)[i];
                    b.pad.process(l, r);
                }
                ++bankIndex;
            }
            for (int i = 0; i < n; ++i) {
                float l = buffer.getSample(0, i), r = buffer.getSample(1, i);
                dry.process(l, r);
                float a = 0, b = 0, currentActivity = 0;
                for (int k = 0; k < 3; ++k) {
                    float weight = quality[size_t(k)].next(k == settings.quality ? 1.0f : 0.0f);
                    currentActivity += weight * bankActivity[size_t(k)];
                    a += weight * banks[size_t(k)].buffer.getSample(0, i);
                    b += weight * banks[size_t(k)].buffer.getSample(1, i);
                }
                // Reconstruction-filter overshoot is caught after downsampling. A sample ceiling, not a
                // true-peak limiter.
                if (isClip) {
                    float ceiling = dbToGain(finalCeiling.next(settings.ceiling));
                    a = std::clamp(a, -ceiling, ceiling);
                    b = std::clamp(b, -ceiling, ceiling);
                }
                float e = enabled.next((isClip ? settings.clipOn
                                               : (settings.colorOn && settings.colorDrive > 0.0001f &&
                                                  settings.colorMix > 0.0001f))
                                           ? 1.0f
                                           : 0.0f);
                activity = std::max(activity, currentActivity * e);
                buffer.setSample(0, i, l + e * (a - l));
                buffer.setSample(1, i, r + e * (b - r));
            }
            return activity;
        }
    };
} // namespace gummy
