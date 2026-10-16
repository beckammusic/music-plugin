#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace gummy {
    struct FactoryPreset {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };
    const std::vector<FactoryPreset> &factoryPresets();
} // namespace gummy
