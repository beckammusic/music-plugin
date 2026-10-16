#include "../Source/DSP/TransientShaper.h"
#include "../Source/DSP/Compressor.h"
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
void requireDynamics(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(std::string("Dynamics: ") + message);
}

float steadyOutput(gummy::Settings settings, float input, double sr = 48000.0) {
    gummy::Compressor compressor;
    compressor.prepare(sr);
    float output = 0;
    for (int i = 0; i < static_cast<int>(sr * 2.0); ++i) {
        float left = input, right = input * .25f;
        compressor.process(left, right, settings);
        output = left;
    }
    return output;
}

void transientTests() {
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
        gummy::TransientShaper shaper;
        shaper.prepare(sr);
        gummy::Settings settings;
        for (int i = 0; i < 10000; ++i) {
            const float inputLeft = .6f * std::sin(static_cast<float>(i) * .173f);
            const float inputRight = -.4f * std::sin(static_cast<float>(i) * .091f);
            float left = inputLeft, right = inputRight;
            shaper.process(left, right, settings);
            requireDynamics(left == inputLeft && right == inputRight, "transient neutral must be sample-exact unity");
        }
    }

    std::array<float, 3> impulse {};
    for (int variant = 0; variant < 3; ++variant) {
        gummy::TransientShaper shaper;
        shaper.prepare(48000);
        gummy::Settings settings;
        settings.attack = static_cast<float>((variant - 1) * 100);
        float left = 1, right = -.5f;
        shaper.process(left, right, settings);
        impulse[static_cast<std::size_t>(variant)] = left;
        requireDynamics(std::abs(left + right * 2) < 1.0e-6f, "transient stereo image must remain linked");
    }
    requireDynamics(impulse[0] < .95f && impulse[1] == 1 && impulse[2] > 1.05f,
                    "attack must soften and enhance impulse onset");

    // A decaying, pitched drum checks onset and tail separately instead of a static transfer.
    std::array<double, 3> tailEnergy {}, onsetEnergy {};
    for (int variant = 0; variant < 3; ++variant) {
        gummy::TransientShaper shaper;
        shaper.prepare(48000);
        gummy::Settings settings;
        settings.sustain = static_cast<float>((variant - 1) * 100);
        for (int i = 0; i < 24000; ++i) {
            const float t = static_cast<float>(i) / 48000.0f;
            float left = .7f * std::exp(-t / .08f) * std::cos(2 * gummy::pi * 90 * t), right = left;
            shaper.process(left, right, settings);
            if (i < 240)
                onsetEnergy[static_cast<std::size_t>(variant)] += left * left;
            if (i > 2400 && i < 10000)
                tailEnergy[static_cast<std::size_t>(variant)] += left * left;
        }
    }
    requireDynamics(tailEnergy[0] < tailEnergy[1] * .6 && tailEnergy[2] > tailEnergy[1] * 1.8,
                    "sustain must tighten and extend a drum decay");
    const double onsetBoost = onsetEnergy[2] / onsetEnergy[1];
    const double tailBoost = tailEnergy[2] / tailEnergy[1];
    requireDynamics(tailBoost > onsetBoost * 2, "sustain action must focus on the body rather than the onset");
}

void compressorTransferTests() {
    gummy::Settings settings;
    settings.threshold = -24;
    settings.ratio = 4;
    settings.compAttack = 1;
    settings.release = 50;
    settings.autoRelease = false;
    for (int mode = 0; mode < 3; ++mode) {
        settings.compType = mode;
        const float below = gummy::dbToGain(-36);
        requireDynamics(std::abs(steadyOutput(settings, below) - below) < 1.0e-6f,
                        "below-knee input must stay unchanged");
        const float highA = gummy::gainToDb(steadyOutput(settings, gummy::dbToGain(-12)));
        const float highB = gummy::gainToDb(steadyOutput(settings, gummy::dbToGain(-6)));
        requireDynamics(std::abs(highA - (-21)) < .03f, "threshold and above-knee gain reduction must be correct");
        requireDynamics(std::abs((highB - highA) - 1.5f) < .03f, "4:1 ratio must map 6 dB input to 1.5 dB output");
        const float knee = mode == 0 ? 6.0f : (mode == 1 ? 1.0f : 4.0f);
        const float atThreshold = gummy::gainToDb(steadyOutput(settings, gummy::dbToGain(-24)));
        const float expected = -24.0f - .75f * knee / 8.0f;
        requireDynamics(std::abs(atThreshold - expected) < .03f, "quadratic soft-knee transfer must match its analytic value");
    }

    settings.compType = 1;
    settings.compMix = 100;
    const float wet = steadyOutput(settings, .5f);
    settings.compMix = 50;
    const float half = steadyOutput(settings, .5f);
    requireDynamics(std::abs(half - .5f * (.5f + wet)) < 2.0e-6f,
                    "parallel compressor mix must combine aligned dry and compressed signals");
    settings.compMix = 0;
    requireDynamics(steadyOutput(settings, .5f) == .5f, "zero compressor mix must be sample-exact dry");
    settings.compMix = 100;
    settings.ratio = 1;
    requireDynamics(steadyOutput(settings, .5f) == .5f, "ratio 1:1 with zero makeup must be unity");
    settings.makeup = 6;
    requireDynamics(std::abs(gummy::gainToDb(steadyOutput(settings, .25f) / .25f) - 6) < .001f,
                    "makeup gain must have the stated dB value");
}

void compressorTimingTests() {
    gummy::Settings settings;
    settings.compType = 1;
    settings.threshold = -24;
    settings.ratio = 4;
    settings.compAttack = 10;
    settings.release = 100;
    settings.autoRelease = false;
    gummy::Compressor compressor;
    compressor.prepare(48000);
    float early = 0, settled = 0;
    for (int i = 0; i < 48000; ++i) {
        float left = 1, right = .05f;
        compressor.process(left, right, settings);
        requireDynamics(std::abs(left - right * 20) < 2.0e-6f, "hot-side detection must apply identical gain to both stereo channels");
        if (i == 479)
            early = compressor.reduction();
        settled = compressor.reduction();
    }
    requireDynamics(early > settled * .2f && early < settled * .7f,
                    "attack must preserve the leading edge and approach steady reduction over time");
    requireDynamics(std::abs(settled - 18) < .03f, "gain-reduction meter must report positive pre-makeup dB");
    for (int i = 0; i < 3360; ++i) { // Punch release = 0.7 * 100 ms; one time constant.
        float left = 0, right = 0;
        compressor.process(left, right, settings);
    }
    requireDynamics(std::abs(compressor.reduction() / settled - std::exp(-1.0f)) < .01f,
                    "manual release must follow its documented one-pole timing");

    std::array<float, 2> attackResponses {};
    for (int test = 0; test < 2; ++test) {
        compressor.reset();
        settings.compAttack = test == 0 ? 1.0f : 100.0f;
        for (int i = 0; i < 480; ++i) {
            float left = 1, right = 1;
            compressor.process(left, right, settings);
        }
        attackResponses[static_cast<std::size_t>(test)] = compressor.reduction();
    }
    requireDynamics(attackResponses[0] > attackResponses[1] * 4,
                    "attack control must change onset response substantially");

    std::array<float, 2> autoRecovery {};
    settings.compType = 0;
    settings.compAttack = 1;
    settings.threshold = -30;
    settings.ratio = 8;
    settings.release = 200;
    settings.autoRelease = true;
    for (int test = 0; test < 2; ++test) {
        compressor.reset();
        const int loudSamples = test == 0 ? 960 : 96000;
        for (int i = 0; i < loudSamples; ++i) {
            float left = 1, right = 1;
            compressor.process(left, right, settings);
        }
        const float initial = compressor.reduction();
        for (int i = 0; i < 12000; ++i) {
            float left = 0, right = 0;
            compressor.process(left, right, settings);
        }
        autoRecovery[static_cast<std::size_t>(test)] = compressor.reduction() / initial;
    }
    requireDynamics(autoRecovery[1] > autoRecovery[0] * 1.2f,
                    "auto release must recover more slowly after sustained compression");

    // Detector topology, rather than only parameter presets, must distinguish modes.
    std::array<double, 3> burstOutput {};
    for (int mode = 0; mode < 3; ++mode) {
        compressor.reset();
        settings.compType = mode;
        settings.autoRelease = false;
        settings.compAttack = .1f;
        for (int i = 0; i < 4800; ++i) {
            float left = i % 480 < 24 ? .8f : 0.0f, right = left;
            compressor.process(left, right, settings);
            burstOutput[static_cast<std::size_t>(mode)] += left * left;
        }
    }
    requireDynamics(std::abs(burstOutput[0] - burstOutput[1]) > burstOutput[0] * .1 &&
                    std::abs(burstOutput[1] - burstOutput[2]) > burstOutput[1] * .05,
                    "Smooth, Punch, and Glue must react differently to a sparse peak train");
}

void dynamicsRobustnessTests() {
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0}) {
        gummy::Compressor compressor;
        gummy::TransientShaper transient;
        compressor.prepare(sr);
        transient.prepare(sr);
        gummy::Settings settings;
        settings.attack = 100;
        settings.sustain = -100;
        settings.threshold = -60;
        settings.ratio = 20;
        settings.compAttack = .1f;
        settings.release = 10;
        for (int i = 0; i < 20000; ++i) {
            settings.compType = (i / 1000) % 3;
            float left = i % 53 == 0 ? 4.0f : 0.0f, right = -left * .3f;
            transient.process(left, right, settings);
            compressor.process(left, right, settings);
            requireDynamics(std::isfinite(left) && std::isfinite(right) && compressor.reduction() >= 0,
                            "extreme dynamics settings and automated modes must stay finite");
        }
        float left = std::numeric_limits<float>::quiet_NaN();
        float right = std::numeric_limits<float>::infinity();
        transient.process(left, right, settings);
        compressor.process(left, right, settings);
        requireDynamics(std::isfinite(left) && std::isfinite(right), "invalid samples must not poison envelope state");
        compressor.reset();
        transient.reset();
        left = right = 0;
        transient.process(left, right, settings);
        compressor.process(left, right, settings);
        requireDynamics(left == 0 && right == 0, "reset dynamics processing must preserve digital silence");
    }
}
} // namespace

void testDynamics() {
    transientTests();
    compressorTransferTests();
    compressorTimingTests();
    dynamicsRobustnessTests();
}
