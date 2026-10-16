#pragma once
#include "Theme.h"

namespace gummy::ui
{
class GummyLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    GummyLookAndFeel()
    {
        setColour (juce::Slider::textBoxTextColourId, ink());
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, lavender().withAlpha (0.25f));
        setColour (juce::ComboBox::textColourId, ink());
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::white);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xffdbd4e4));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xfffaf8fd));
        setColour (juce::PopupMenu::textColourId, ink());
        setColour (juce::PopupMenu::highlightedBackgroundColourId, lavender().withAlpha (0.22f));
        setColour (juce::PopupMenu::highlightedTextColourId, ink());
        setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xfffaf8fd));
        setColour (juce::TooltipWindow::textColourId, ink());
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xffd7cee3));
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        setColour (juce::TextEditor::textColourId, ink());
        setColour (juce::TextEditor::outlineColourId, lavender());
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                           float start, float end, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (7.0f);
        const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        bounds = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
        auto centre = bounds.getCentre();
        const auto radius = size * 0.5f;
        auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const float angle = start + position * (end - start);

        juce::Path track, arc;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
        g.setColour (juce::Colour (0xffded9e7));
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        auto* parameterIsBipolar = slider.getProperties().getVarPointer ("bipolar");
        const auto neutral = (float) slider.getProperties().getWithDefault ("neutralProportion", 0.5f);
        const auto from = parameterIsBipolar != nullptr && (bool) *parameterIsBipolar ? start + neutral * (end - start) : start;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, from, angle, true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        auto face = bounds.reduced (6.0f);
        juce::Path shadow;
        shadow.addEllipse (face);
        juce::DropShadow (juce::Colour (0xff817493).withAlpha (0.24f), 7, { 0, 4 }).drawForPath (g, shadow);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, face.getX(), face.getY(),
                                                juce::Colour (0xffd8d3e1), face.getRight(), face.getBottom(), false));
        g.fillEllipse (face);
        g.setColour (juce::Colours::white);
        g.drawEllipse (face.reduced (0.5f), 1.3f);
        auto inner = face.reduced (3.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffdfcff), inner.getX(), inner.getY(),
                                                accent.interpolatedWith (juce::Colour (0xffeae7f0), 0.85f),
                                                inner.getRight(), inner.getBottom(), false));
        g.fillEllipse (inner);
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.fillEllipse (inner.withHeight (inner.getHeight() * 0.48f).reduced (4.0f, 1.0f));

        juce::Path pointer;
        pointer.addRoundedRectangle (-1.8f, -face.getHeight() * 0.39f, 3.6f, face.getHeight() * 0.19f, 1.8f);
        g.setColour (accent.darker (0.22f));
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        if (slider.hasKeyboardFocus (true))
        {
            g.setColour (ink().withAlpha (0.55f));
            g.drawEllipse (bounds.expanded (3.0f), 1.0f);
        }
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);
        label->setFont (font (12.0f, true));
        label->setJustificationType (juce::Justification::centred);
        label->setBorderSize (juce::BorderSize<int> (0));
        label->setColour (juce::Label::textColourId, ink());
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
        label->setColour (juce::TextEditor::textColourId, ink());
        label->setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        label->setColour (juce::TextEditor::outlineColourId, lavender());
        label->setColour (juce::TextEditor::focusedOutlineColourId, lavender().darker (0.15f));
        label->setColour (juce::TextEditor::highlightColourId, lavender().withAlpha (0.25f));
        label->setColour (juce::TextEditor::highlightedTextColourId, ink());
        return label;
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPosition, float, float, juce::Slider::SliderStyle,
                           juce::Slider& slider) override
    {
        const auto middle = (float) y + (float) height * 0.5f;
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        auto track = juce::Rectangle<float> ((float) x + 5.0f, middle - 2.0f, (float) width - 10.0f, 4.0f);
        g.setColour (juce::Colour (0xffddd6e6));
        g.fillRoundedRectangle (track, 2.0f);
        g.setColour (accent);
        g.fillRoundedRectangle (track.withRight (juce::jlimit (track.getX(), track.getRight(), sliderPosition)), 2.0f);
        auto thumb = juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ sliderPosition, middle });
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0, thumb.getY(),
                                                accent.interpolatedWith (juce::Colours::white, 0.4f), 0, thumb.getBottom(), false));
        g.fillEllipse (thumb);
        g.setColour (accent.darker (0.2f));
        g.drawEllipse (thumb, 0.8f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool over, bool down) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (0.75f);
        const auto selected = button.getToggleState();
        const auto accent = button.findColour (juce::TextButton::buttonOnColourId);
        auto top = selected ? accent.brighter (0.25f) : juce::Colour (0xfffefcff);
        auto bottom = selected ? accent : juce::Colour (0xffebe6f1);
        if (over) top = top.brighter (0.05f);
        if (down) bottom = bottom.darker (0.06f);
        g.setGradientFill (juce::ColourGradient (top, 0, r.getY(), bottom, 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, juce::jmin (10.0f, r.getHeight() * 0.4f));
        g.setColour (selected ? accent.darker (0.12f).withAlpha (0.6f) : juce::Colour (0xffd5cedf));
        g.drawRoundedRectangle (r, juce::jmin (10.0f, r.getHeight() * 0.4f), 0.8f);
        if (button.hasKeyboardFocus (false))
        {
            g.setColour (ink().withAlpha (0.6f));
            g.drawRoundedRectangle (r.reduced (2.0f), 7.0f, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour (ink());
        g.setFont (font (button.getHeight() < 27 ? 10.5f : 12.0f, true));
        g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (3, 0), juce::Justification::centred, 1);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, 0, 0,
                                                juce::Colour (0xfff0edf5), 0, (float) height, false));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour (0xffd7d0e2));
        g.drawRoundedRectangle (r, 8.0f, 0.8f);
        juce::Path chevron;
        const auto cx = (float) width - 13.0f;
        const auto cy = (float) height * 0.5f;
        chevron.startNewSubPath (cx - 3, cy - 1.5f);
        chevron.lineTo (cx, cy + 1.5f);
        chevron.lineTo (cx + 3, cy - 1.5f);
        g.setColour (muted());
        g.strokePath (chevron, juce::PathStrokeType (1.3f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (11.5f, true); }
    juce::Font getPopupMenuFont() override { return font (13.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (9, 0, box.getWidth() - 30, box.getHeight());
        label.setFont (getComboBoxFont (box));
    }
};
}
