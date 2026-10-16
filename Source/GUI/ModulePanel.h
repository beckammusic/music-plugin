#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace gummy::ui
{
class ModulePanel final : public juce::Component
{
public:
    ModulePanel (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                 juce::String number, juce::String name, juce::String detail, juce::Colour colour)
        : index (std::move (number)), title (std::move (name)), subtitle (std::move (detail)), accent (colour)
    {
        addAndMakeVisible (power);
        power.setClickingTogglesState (true);
        power.setComponentID (id);
        power.setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.58f));
        power.setTooltip ("Enable or bypass " + title.toLowerCase() + ". Settings are kept while bypassed.");
        power.setTitle (title + " enabled");
        power.onStateChange = [this]
        {
            power.setButtonText (power.getToggleState() ? "ON" : "BYP");
            repaint();
        };
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, power);
        power.setButtonText (power.getToggleState() ? "ON" : "BYP");
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        card (g, r);
        g.setColour (accent.withAlpha (0.12f));
        g.fillRoundedRectangle (13.0f, 14.0f, 24.0f, 22.0f, 7.0f);
        g.setColour (accent.darker (0.30f));
        g.setFont (font (9.5f, true));
        g.drawText (index, 13, 14, 24, 22, juce::Justification::centred);
        g.setColour (ink());
        g.setFont (font (14.0f, true));
        g.drawText (title, 45, 13, getWidth() - 107, 23, juce::Justification::centredLeft);
        g.setColour (muted());
        g.setFont (font (9.0f));
        g.drawText (subtitle, 15, getHeight() - 23, getWidth() - 30, 13, juce::Justification::centredLeft);
    }
    void resized() override { power.setBounds (getWidth() - 52, 15, 37, 21); }
private:
    juce::String index, title, subtitle;
    juce::Colour accent;
    juce::TextButton power;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};
}
