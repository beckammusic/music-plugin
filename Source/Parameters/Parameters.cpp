#include "Parameters.h"
namespace gummy {
    const std::vector<ParameterSpec> &parameterSpecs() {
        static const std::vector<ParameterSpec> specs{
            {"input", "Input", -24, 24, 0, 1, "dB", ""},
            {"output", "Output", -24, 24, 0, 1, "dB", ""},
            {"mix", "Global Mix", 0, 100, 100, 1, "%", ""},
            {"bypass", "Master Bypass", 0, 1, 0, 1, "", "bool"},
            {"quality", "Oversampling", 0, 2, 1, 1, "", "Off|2x|4x"},
            {"transientOn", "Transient Enabled", 0, 1, 1, 1, "", "bool"},
            {"attack", "Transient Attack", -100, 100, 0, 1, "", ""},
            {"sustain", "Transient Sustain", -100, 100, 0, 1, "", ""},
            {"compOn", "Compressor Enabled", 0, 1, 1, 1, "", "bool"},
            {"threshold", "Threshold", -60, 0, 0, 1, "dB", ""},
            {"ratio", "Ratio", 1, 20, 1, .5f, ":1", ""},
            {"compAttack", "Compressor Attack", .1f, 100, 10, .35f, "ms", ""},
            {"release", "Release", 10, 2000, 150, .35f, "ms", ""},
            {"makeup", "Makeup", -12, 24, 0, 1, "dB", ""},
            {"autoRelease", "Auto Release", 0, 1, 1, 1, "", "bool"},
            {"compMix", "Compressor Mix", 0, 100, 100, 1, "%", ""},
            {"compType", "Compressor Type", 0, 2, 0, 1, "", "Smooth|Punch|Glue"},
            {"colorOn", "Color Enabled", 0, 1, 1, 1, "", "bool"},
            {"colorDrive", "Color Drive", 0, 100, 0, 1, "", ""},
            {"colorMix", "Color Mix", 0, 100, 100, 1, "%", ""},
            {"colorType", "Color Type", 0, 2, 0, 1, "", "Tape|Tube|Color"},
            {"toneOn", "Tone Enabled", 0, 1, 1, 1, "", "bool"},
            {"body", "Body", -100, 100, 0, 1, "", ""},
            {"presence", "Presence", -100, 100, 0, 1, "", ""},
            {"air", "Air", -100, 100, 0, 1, "", ""},
            {"smoothOn", "Smooth Enabled", 0, 1, 1, 1, "", "bool"},
            {"smooth", "Resonance Suppression", 0, 100, 0, 1, "", ""},
            {"motionOn", "Motion Enabled", 0, 1, 1, 1, "", "bool"},
            {"motionAmount", "Motion Amount", 0, 100, 0, 1, "", ""},
            {"motionType", "Motion Type", 0, 2, 0, 1, "", "Chorus|Flanger|Doubler"},
            {"clipOn", "Clip Enabled", 0, 1, 0, 1, "", "bool"},
            {"clipDrive", "Clip Drive", 0, 24, 0, 1, "dB", ""},
            {"ceiling", "Clip Ceiling", -12, 0, 0, 1, "dBFS", ""},
            {"clipMode", "Clip Mode", 0, 1, 0, 1, "", "Hard|Soft"}};
        return specs;
    }
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout() {
        juce::AudioProcessorValueTreeState::ParameterLayout result;
        for (const auto &p : parameterSpecs()) {
            juce::ParameterID id(p.id, 1);
            if (juce::String(p.choices) == "bool")
                result.add(std::make_unique<juce::AudioParameterBool>(id, p.name, p.initial > .5f));
            else if (*p.choices)
                result.add(std::make_unique<juce::AudioParameterChoice>(
                    id, p.name, juce::StringArray::fromTokens(p.choices, "|", ""), int(p.initial)));
            else {
                auto attr =
                    juce::AudioParameterFloatAttributes()
                        .withLabel(p.unit)
                        .withStringFromValueFunction([unit = juce::String(p.unit)](float v, int) {
                            return juce::String(v, std::abs(v) < 10 ? 2 : 1) +
                                   (unit.isEmpty() ? "" : " " + unit);
                        })
                        .withValueFromStringFunction([](const juce::String &t) { return t.getFloatValue(); });
                result.add(std::make_unique<juce::AudioParameterFloat>(
                    id, p.name, juce::NormalisableRange<float>(p.lo, p.hi, .001f, p.skew), p.initial, attr));
            }
        }
        return result;
    }
    ParameterReader::ParameterReader(juce::AudioProcessorValueTreeState &state) {
        for (const auto &p : parameterSpecs())
            values.push_back(state.getRawParameterValue(p.id));
    }
    Settings ParameterReader::read() const noexcept {
        Settings s;
        size_t i = 0;
#define READ(field) s.field = static_cast<decltype(s.field)>(values[i++]->load(std::memory_order_relaxed));
        READ(input)
        READ(output) READ(mix) READ(bypass) READ(quality) READ(transientOn) READ(attack) READ(sustain)
            READ(compOn) READ(threshold) READ(ratio) READ(compAttack) READ(release) READ(makeup)
                READ(autoRelease) READ(compMix) READ(compType) READ(colorOn) READ(colorDrive) READ(colorMix)
                    READ(colorType) READ(toneOn) READ(body) READ(presence) READ(air) READ(smoothOn)
                        READ(smooth) READ(motionOn) READ(motionAmount) READ(motionType) READ(clipOn)
                            READ(clipDrive) READ(ceiling) READ(clipMode)
#undef READ
                                return s;
    }
} // namespace gummy
