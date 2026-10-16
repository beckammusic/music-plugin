#pragma once
#include "Compressor.h"
#include "Modulation.h"
#include "MusicalEQ.h"
#include "Oversampling.h"
#include "ResonanceSuppressor.h"
#include "TransientShaper.h"
namespace gummy {
    class Engine {
        TransientShaper transient;
        Compressor compressor;
        MusicalEQ tone;
        ResonanceSuppressor resonance;
        Modulation motion;
        NonlinearStage saturation, clipper;
        juce::AudioBuffer<float> work;
        std::array<FixedDelay, 1> globalDry;
        std::array<Slew, 5> module;
        Slew input, output, mix, bypass;
        int capacity = 512, latency_ = 0;

      public:
    static constexpr int expectedLatency = 120;
        void prepare(double sr, int maxBlock) {
            sr = std::isfinite(sr) && sr >= 8000 && sr <= 384000 ? sr : 48000;
            capacity = std::max(1, maxBlock);
            work.setSize(2, capacity);
            transient.prepare(sr);
            compressor.prepare(sr);
            tone.prepare(sr);
            resonance.prepare(sr);
            motion.prepare(sr);
            saturation.prepare(sr, capacity, false);
            clipper.prepare(sr, capacity, true);
            latency_ = saturation.latency() + clipper.latency();
            globalDry[0].reset(latency_);
            for (auto &s : module)
                s.prepare(sr, 8);
            input.prepare(sr);
            output.prepare(sr);
            mix.prepare(sr);
            bypass.prepare(sr, 8);
        }
        int latency() const noexcept { return latency_; }
        MeterValues process(juce::AudioBuffer<float> &buffer, const Settings &s,
                            bool hostBypass = false) noexcept {
            MeterValues meters;
            int channels = buffer.getNumChannels();
            int total = buffer.getNumSamples();
            for (int offset = 0; offset < total; offset += capacity) {
                int n = std::min(capacity, total - offset);
                for (int i = 0; i < n; ++i) {
                    float l = finite(buffer.getSample(0, offset + i)),
                          r = channels > 1 ? finite(buffer.getSample(1, offset + i)) : l;
                    meters.input = std::max({meters.input, std::abs(l), std::abs(r)});
                    float g = dbToGain(input.next(s.input));
                    l *= g;
                    r *= g;
                    float a = l, b = r;
                    transient.process(a, b, s);
                    float e = module[0].next(s.transientOn ? 1.0f : 0.0f);
                    l += e * (a - l);
                    r += e * (b - r);
                    a = l;
                    b = r;
                    compressor.process(a, b, s);
                    e = module[1].next(s.compOn ? 1.0f : 0.0f);
                    l += e * (a - l);
                    r += e * (b - r);
                    if (s.compOn)
                        meters.reduction = std::max(meters.reduction, compressor.reduction());
                    work.setSample(0, i, l);
                    work.setSample(1, i, r);
                }
                saturation.process(work, n, s);
                for (int i = 0; i < n; ++i) {
                    float l = work.getSample(0, i), r = work.getSample(1, i), a = l, b = r;
                    tone.process(a, b, s);
                    float e = module[2].next(s.toneOn ? 1.0f : 0.0f);
                    l += e * (a - l);
                    r += e * (b - r);
                    a = l;
                    b = r;
                    resonance.process(a, b, s);
                    e = module[3].next(s.smoothOn ? 1.0f : 0.0f);
                    l += e * (a - l);
                    r += e * (b - r);
                    if (s.smoothOn)
                        meters.resonance = std::max(meters.resonance, resonance.activity());
                    a = l;
                    b = r;
                    motion.process(a, b, s);
                    e = module[4].next(s.motionOn ? 1.0f : 0.0f);
                    l += e * (a - l);
                    r += e * (b - r);
                    work.setSample(0, i, l);
                    work.setSample(1, i, r);
                }
                float clipActivity = clipper.process(work, n, s);
                if (s.clipOn)
                    meters.clip = std::max(meters.clip, clipActivity);
                for (int i = 0; i < n; ++i) {
                    float l = work.getSample(0, i), r = work.getSample(1, i);
                    float dl = finite(buffer.getSample(0, offset + i)),
                          dr = channels > 1 ? finite(buffer.getSample(1, offset + i)) : dl;
                    globalDry[0].process(dl, dr);
                    float gain = dbToGain(output.next(s.output));
                    l *= gain;
                    r *= gain;
                    float wet = mix.next(std::clamp(s.mix * .01f, 0.0f, 1.0f)) *
                                (1 - bypass.next((hostBypass || s.bypass) ? 1.0f : 0.0f));
                    l = finite(dl + wet * (l - dl));
                    r = finite(dr + wet * (r - dr));
                    buffer.setSample(0, offset + i, l);
                    if (channels > 1)
                        buffer.setSample(1, offset + i, r);
                    meters.output = std::max({meters.output, std::abs(l), std::abs(r)});
                }
            }
            return meters;
        }
    };
} // namespace gummy
