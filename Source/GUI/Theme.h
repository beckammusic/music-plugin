#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace gummy::ui
{
inline juce::Colour ink()       { return juce::Colour (0xff39354b); }
inline juce::Colour muted()     { return juce::Colour (0xff79758b); }
inline juce::Colour pink()      { return juce::Colour (0xffdf8fba); }
inline juce::Colour blue()      { return juce::Colour (0xff8eb8db); }
inline juce::Colour lavender()  { return juce::Colour (0xffaa9fd8); }
inline juce::Colour mint()      { return juce::Colour (0xff81b7a6); }
inline juce::Colour peach()     { return juce::Colour (0xffdfa78b); }
inline juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
inline void card (juce::Graphics& g, juce::Rectangle<float> r, float radius = 19.0f)
{
    juce::Path shadow;
    shadow.addRoundedRectangle (r, radius);
    juce::DropShadow (juce::Colour (0xff71658c).withAlpha (0.10f), 12, { 0, 4 }).drawForPath (g, shadow);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.97f), r.getX(), r.getY(),
                                            juce::Colour (0xfff0edf5), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::white.withAlpha (0.95f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colour (0xffc8c1d4).withAlpha (0.42f));
    g.drawRoundedRectangle (r.reduced (1.2f), radius - 1.0f, 0.6f);
}
}
