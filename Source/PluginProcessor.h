#pragma once
#include "DSP/Engine.h"
#include "Parameters/Parameters.h"
#include <juce_audio_processors/juce_audio_processors.h>
class GummyAudioProcessor final : public juce::AudioProcessor,
                                  private juce::AudioProcessorValueTreeState::Listener {
  public:
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    GummyAudioProcessor();
    ~GummyAudioProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout &) const override;
    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    void processBlockBypassed(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "GUMMY"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return .15; }
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override;
    void setStateInformation(const void *, int) override;
    juce::AudioProcessorParameter *getBypassParameter() const override;
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float> inputMeter{0}, outputMeter{0}, reductionMeter{0}, resonanceMeter{0}, clipMeter{0};
    juce::StringArray presetNames() const;
    int currentPreset() const { return presetIndex.load(); }
    juce::String presetDisplayName() const;
    void loadFactoryPreset(int);
    bool saveUserPreset(const juce::File &);
    bool loadUserPreset(const juce::File &);
    void switchAB();
    bool isSlotB() const { return slotB.load(); }

  private:
    gummy::ParameterReader reader;
    gummy::Engine engine;
    std::atomic<int> presetIndex{0};
    std::atomic<bool> modified{false}, loading{false}, slotB{false};
    juce::ValueTree slots[2];
    mutable juce::CriticalSection stateLock;
    juce::String userPresetName{"USER PRESET"};
    double meterSampleRate = 48000;
    void parameterChanged(const juce::String &, float) override;
    void render(juce::AudioBuffer<float> &, bool);
    juce::ValueTree snapshot();
    bool restore(const juce::ValueTree &);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GummyAudioProcessor)
};
