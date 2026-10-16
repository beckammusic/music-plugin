#include "../Source/DSP/MusicalEQ.h"
#include "../Source/DSP/ResonanceSuppressor.h"
#include "../Source/DSP/Modulation.h"
#include <stdexcept>
#include <string>
#include <limits>

namespace {
void requireAdvanced(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("Advanced DSP: ") + message);
}
float eqGain(double sr, double hz, int macro, float setting) {
    gummy::MusicalEQ eq; eq.prepare(sr);
    gummy::Settings settings;
    if (macro == 0) settings.body = setting;
    if (macro == 1) settings.presence = setting;
    if (macro == 2) settings.air = setting;
    double input = 0, output = 0;
    const int total = static_cast<int>(sr * .24);
    for (int i = 0; i < total; ++i) {
        float l = .1f * std::sin(float(2.0 * double(gummy::pi) * hz * i / sr)), r = l;
        const float dry = l;
        eq.process(l, r, settings);
        if (i > total / 2) { input += dry * dry; output += l * l; }
        requireAdvanced(std::abs(l - r) < 1.0e-6f, "EQ stereo consistency");
    }
    return 10.0f * std::log10(static_cast<float>(output / input));
}
// Deterministic noise avoids implementation-specific random distributions.
float noiseAdvanced(unsigned& seed) {
    seed = seed * 1664525u + 1013904223u;
    return float((seed >> 8) & 0x00ffffffu) / 8388608.0f - 1.0f;
}
}

void testAdvanced() {
    for (const double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
        gummy::Settings settings;
        gummy::MusicalEQ eq; eq.prepare(sr);
        gummy::Modulation motion; motion.prepare(sr);
        gummy::ResonanceSuppressor smooth; smooth.prepare(sr);
        unsigned seed = 17;
        for (int i = 0; i < 4096; ++i) {
            const float originalL = noiseAdvanced(seed) * .25f, originalR = noiseAdvanced(seed) * .25f;
            float l = originalL, r = originalR;
            eq.process(l, r, settings);
            requireAdvanced(std::abs(l - originalL) < 1.0e-6f && std::abs(r - originalR) < 1.0e-6f,
                            "neutral EQ must preserve signal");
            motion.process(l, r, settings);
            requireAdvanced(l == originalL && r == originalR, "zero motion must be exactly dry");
            smooth.process(l, r, settings);
            requireAdvanced(l == originalL && r == originalR, "zero smooth must be exactly dry");
        }
        requireAdvanced(std::abs(eqGain(sr, 2800, 1, 100) - 9.0f) < .15f, "presence boost response");
        requireAdvanced(std::abs(eqGain(sr, 2800, 1, -100) + 9.0f) < .15f, "presence cut response");
        const float body = eqGain(sr, 50, 0, 100), air = eqGain(sr, 17000, 2, 100);
        requireAdvanced(body > 7.0f && body < 9.2f, "broad body boost range");
        requireAdvanced(air > 7.0f && air < 9.2f, "air shelf boost range");
        requireAdvanced(std::abs(eqGain(sr, 50, 0, -100) + body) < .15f, "body reciprocal cut");
        requireAdvanced(std::abs(eqGain(sr, 17000, 2, -100) + air) < .15f, "air reciprocal cut");

        // Continuous history plus a smooth selector must not step between modes.
        settings.motionAmount = 100;
        for (int i = 0; i < static_cast<int>(sr * .4); ++i) {
            float l = .1f, r = .1f; motion.process(l, r, settings);
        }
        float previous = 0;
        for (int i = 0; i < static_cast<int>(sr * .3); ++i) {
            settings.motionType = (i / 113) % 3;
            float l = .1f, r = .1f; motion.process(l, r, settings);
            requireAdvanced(std::isfinite(l) && std::isfinite(r), "modulation mode automation finite");
            if (i > 0) requireAdvanced(std::abs(l - previous) < .001f, "modulation selector discontinuity");
            previous = l;
        }
        // A full-scale impulse must decay with maximum flanger feedback.
        motion.reset(); settings.motionType = 1;
        double tail = 0;
        for (int i = 0; i < static_cast<int>(sr * 2); ++i) {
            float l = i == 0 ? 1.0f : 0.0f, r = l;
            motion.process(l, r, settings);
            requireAdvanced(std::isfinite(l) && std::abs(l) < 2, "flanger feedback stability");
            if (i > static_cast<int>(sr * 1.8)) tail += l * l + r * r;
        }
        requireAdvanced(tail < 1.0e-8, "flanger tail must decay");

        for (int mode = 0; mode < 3; ++mode) {
            motion.reset(); settings.motionType = mode; settings.motionAmount = 100;
            double difference = 0, stereo = 0, mono = 0;
            for (int i = 0; i < static_cast<int>(sr * .20); ++i) {
                const float dry = .15f * std::sin(float(2.0 * double(gummy::pi) * 997 * i / sr));
                float l = dry, r = dry; motion.process(l, r, settings);
                if (i > static_cast<int>(sr * .10)) {
                    difference += (l - dry) * (l - dry);
                    stereo += (l - r) * (l - r);
                    mono += .25 * (l + r) * (l + r);
                }
            }
            requireAdvanced(difference > sr * 1.0e-5, "each modulation mode must produce an audible wet signal");
            requireAdvanced(stereo > sr * 1.0e-6, "each modulation mode must produce stereo movement");
            requireAdvanced(mono > sr * 1.0e-5, "modulation must preserve signal in mono sum");
        }

        // Project onto two frequencies to measure a resonance independently of
        // its broadband background and verify that a remote desired tone stays.
        smooth.reset(); settings.smooth = 100;
        double inResonance = 0, outResonance = 0, inReference = 0, outReference = 0;
        float activity = 0;
        for (int i = 0; i < static_cast<int>(sr); ++i) {
            const float resonant = std::sin(float(2.0 * double(gummy::pi) * 3000 * i / sr));
            const float reference = std::sin(float(2.0 * double(gummy::pi) * 500 * i / sr));
            const float dry = .3f * resonant + .025f * reference + .1f * noiseAdvanced(seed);
            float l = dry, r = dry * .5f; smooth.process(l, r, settings);
            requireAdvanced(std::abs(r - .5f * l) < 1.0e-6f, "resonance stereo-linked attenuation");
            if (i > static_cast<int>(sr * .5)) {
                inResonance += dry * resonant; outResonance += l * resonant;
                inReference += dry * reference; outReference += l * reference;
                activity = std::max(activity, smooth.activity());
            }
        }
        requireAdvanced(outResonance / inResonance < .85, "resonance should attenuate a narrow peak in broadband material");
        requireAdvanced(std::abs(20.0 * std::log10(outReference / inReference)) < .7,
                        "resonance should preserve a remote desired tone");
        requireAdvanced(activity > 1 && activity < 9.1f, "resonance activity should report bounded reduction");

        // Once amount reaches zero, existing attenuation fades to an exact dry
        // path; an isolated musical sine must not be assumed to be resonance.
        settings.smooth = 0;
        bool dryAgain = false;
        for (int i = 0; i < static_cast<int>(sr * .6); ++i) {
            const float dry = .15f * noiseAdvanced(seed);
            float l = dry, r = dry; smooth.process(l, r, settings);
            if (i == static_cast<int>(sr * .6) - 1) dryAgain = l == dry && r == dry;
        }
        requireAdvanced(dryAgain && smooth.activity() == 0, "resonance zero automation must settle to exact dry");
        smooth.reset(); settings.smooth = 100;
        double plainIn = 0, plainOut = 0;
        for (int i = 0; i < static_cast<int>(sr * .25); ++i) {
            const float dry = .15f * noiseAdvanced(seed);
            float l = dry, r = dry; smooth.process(l, r, settings);
            if (i > static_cast<int>(sr * .1)) { plainIn += dry * dry; plainOut += l * l; }
        }
        requireAdvanced(std::abs(10.0 * std::log10(plainOut / plainIn)) < .1,
                        "broadband material must not be globally dulled");
        smooth.reset(); plainIn = plainOut = 0;
        for (int i = 0; i < static_cast<int>(sr * .25); ++i) {
            const float dry = .3f * std::sin(float(2.0 * double(gummy::pi) * 3000 * i / sr));
            float l = dry, r = dry; smooth.process(l, r, settings);
            if (i > static_cast<int>(sr * .1)) { plainIn += dry * dry; plainOut += l * l; }
        }
        requireAdvanced(std::abs(10.0 * std::log10(plainOut / plainIn)) < .1,
                        "isolated steady tones must not be assumed to be unwanted resonance");

        // Extreme automation stresses coefficient interpolation, independent
        // channel states, high-rate delays, and bounded resonance reduction.
        eq.reset(); smooth.reset(); motion.reset();
        for (int i = 0; i < static_cast<int>(sr * .25); ++i) {
            settings.body = ((i / 29) & 1) ? 100 : -100;
            settings.presence = -settings.body; settings.air = settings.body;
            settings.smooth = ((i / 31) & 1) ? 100 : 0;
            settings.motionAmount = ((i / 37) & 1) ? 100 : 0;
            settings.motionType = (i / 41) % 3;
            float l = noiseAdvanced(seed) * .7f, r = noiseAdvanced(seed) * .7f;
            eq.process(l, r, settings); smooth.process(l, r, settings); motion.process(l, r, settings);
            requireAdvanced(std::isfinite(l) && std::isfinite(r) && std::abs(l) < 12 && std::abs(r) < 12,
                            "automated advanced chain stability");
            requireAdvanced(smooth.activity() >= 0 && smooth.activity() <= 12.1f,
                            "resonance meter range");
        }
    }
}
