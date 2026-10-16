#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "GUI/GummyLookAndFeel.h"
#include "GUI/GummyKnob.h"
#include "GUI/ModulePanel.h"
#include "GUI/MeterComponent.h"

class GummyAudioProcessor;

class GummyAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GummyAudioProcessorEditor (GummyAudioProcessor&);
    ~GummyAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void choosePresetFile (bool saving);
    void stepPreset (int direction);
    void showStatus (const juce::String&);
    gummy::ui::GummyKnob& addKnob (juce::Component&, const char* id, const char* label,
                                   const char* help, juce::Colour, bool bipolar = false);

    GummyAudioProcessor& processor;
    gummy::ui::GummyLookAndFeel lookAndFeel;
    juce::Component canvas;
    juce::TooltipWindow tooltip;
    std::unique_ptr<gummy::ui::ModulePanel> transientPanel, compressPanel, colorPanel,
                                                tonePanel, smoothPanel, motionPanel, clipPanel;
    std::vector<std::unique_ptr<gummy::ui::GummyKnob>> knobs;
    std::unique_ptr<gummy::ui::ModeSelector> compModes, colorModes, motionModes, clipModes;
    juce::ComboBox presetSelector, qualitySelector;
    juce::TextButton previous { "<" }, next { ">" }, save { "SAVE" }, load { "LOAD" },
                     comparison { "A / B" }, bypass { "BYPASS" }, autoRelease { "AUTO RELEASE" };
    juce::Label status;
    gummy::ui::MeterComponent inputMeter { "INPUT", gummy::ui::blue(), true },
                               outputMeter { "OUTPUT", gummy::ui::pink(), true },
                               reductionMeter { "GAIN REDUCTION", gummy::ui::lavender() },
                               resonanceMeter { "RESONANCE ACTIVITY", gummy::ui::mint() },
                               clippingMeter { "CLIP ACTIVITY", gummy::ui::peach() };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> qualityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment, autoReleaseAttachment;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String lastPresetName;
    int statusTicks = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GummyAudioProcessorEditor)
};
