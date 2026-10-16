#pragma once
#include "../DSP/Common.h"
#include <juce_audio_processors/juce_audio_processors.h>
namespace gummy {
    struct ParameterSpec {
        const char *id;
        const char *name;
        float lo, hi, initial, skew;
        const char *unit;
        const char *choices;
    };
    const std::vector<ParameterSpec> &parameterSpecs();
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    class ParameterReader {
      public:
        explicit ParameterReader(juce::AudioProcessorValueTreeState &);
        Settings read() const noexcept;

      private:
        std::vector<std::atomic<float> *> values;
    };
} // namespace gummy
