#pragma once
#include "Theme.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace gummy::ui
{
class MeterComponent final : public juce::Component
{
public:
    MeterComponent (juce::String name, juce::Colour colour, bool isLevel = false)
        : title (std::move (name)), accent (colour), levelMeter (isLevel)
    {
        setTitle (title + " meter");
        setInterceptsMouseClicks (false, false);
    }
    void setReading (float raw)
    {
        if (! std::isfinite (raw)) raw = 0.0f;
        const auto value = levelMeter ? juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (raw, -60.0f) + 60.0f) / 60.0f)
                                     : juce::jlimit (0.0f, 1.0f, raw / 18.0f);
        shown += (value > shown ? 0.75f : 0.12f) * (value - shown);
        reading = raw;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setFont (font (9.0f, true));
        g.setColour (muted());
        g.drawText (title, r.removeFromTop (13.0f), juce::Justification::centredLeft);
        auto bar = r.withHeight (6.0f).withY (r.getY() + 4.0f);
        g.setColour (juce::Colour (0xffddd7e5));
        g.fillRoundedRectangle (bar, 3.0f);
        if (shown > 0.002f)
        {
            g.setGradientFill (juce::ColourGradient (accent.brighter (0.2f), bar.getX(), 0,
                                                    accent, bar.getRight(), 0, false));
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * shown), 3.0f);
        }
        g.setFont (font (9.0f));
        g.setColour (levelMeter && reading > 1.0f ? juce::Colour (0xffb34c75) : muted());
        const auto value = levelMeter ? juce::Decibels::gainToDecibels (reading, -60.0f) : -reading;
        g.drawText ((levelMeter && reading < 0.001f ? juce::String ("-inf") : juce::String (value, 1)) + " dB",
                    getWidth() - 60, 0, 60, 13, juce::Justification::centredRight);
    }
private:
    juce::String title;
    juce::Colour accent;
    bool levelMeter;
    float shown = 0.0f, reading = 0.0f;
};
}
