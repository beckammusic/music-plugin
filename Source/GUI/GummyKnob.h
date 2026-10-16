#pragma once
#include "Theme.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace gummy::ui
{
class EditableRotary final : public juce::Slider
{
public:
    double resetValue = 0.0;
    std::function<void()> resetParameter;
    void mouseDoubleClick (const juce::MouseEvent&) override { showTextBox(); }
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isPopupMenu())
        {
            juce::Slider::mouseDown (event);
            return;
        }
        juce::PopupMenu menu;
        menu.addItem (1, "Reset to default");
        menu.addItem (2, "Enter value...");
        auto safe = juce::Component::SafePointer<EditableRotary> (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int choice)
        {
            if (safe == nullptr) return;
            if (choice == 1)
            {
                if (safe->resetParameter) safe->resetParameter();
                else safe->setValue (safe->resetValue, juce::sendNotificationSync);
            }
            if (choice == 2) safe->showTextBox();
        });
    }
};

class GummyKnob final : public juce::Component
{
public:
    GummyKnob (juce::AudioProcessorValueTreeState& state, const juce::String& id,
               juce::String name, juce::String help, juce::Colour accent, bool bipolar = false)
        : title (std::move (name))
    {
        addAndMakeVisible (slider);
        // Text boxes are first created before the knob joins the editor tree.
        // Keep their colours explicit so the default dark JUCE theme cannot
        // leave cached white text on GUMMY's pale panels.
        slider.setColour (juce::Slider::textBoxTextColourId, ink());
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxHighlightColourId, lavender().withAlpha (0.25f));
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.22f, juce::MathConstants<float>::pi * 2.78f, true);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 88, 20);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.getProperties().set ("bipolar", bipolar);
        slider.setMouseDragSensitivity (175);
        slider.setScrollWheelEnabled (true);
        slider.setWantsKeyboardFocus (true);
        slider.setComponentID (id);
        slider.setTitle (title);
        slider.setDescription (help);
        slider.setTooltip (help + "  Drag to adjust; Shift-drag for fine control. Double-click to type. Right-click to reset.");
        slider.setVelocityModeParameters (0.35, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
        if (auto* parameter = state.getParameter (id))
        {
            slider.setTitle (parameter->getName (64));
            slider.resetValue = parameter->convertFrom0to1 (parameter->getDefaultValue());
            slider.resetParameter = [parameter]
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
                parameter->endChangeGesture();
            };
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, id, slider);
            if (bipolar) slider.getProperties().set ("neutralProportion", parameter->convertTo0to1 (0.0f));
            // JUCE's attachment enables double-click reset; GUMMY reserves that
            // gesture for precise entry and offers an explicit reset menu.
            slider.setDoubleClickReturnValue (false, slider.resetValue);
        }
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (ink());
        g.setFont (font (10.5f, true));
        g.drawText (title, 0, 0, getWidth(), 16, juce::Justification::centred);
    }
    void resized() override { slider.setBounds (0, 15, getWidth(), getHeight() - 15); }
    EditableRotary slider;
private:
    juce::String title;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class ModeSelector final : public juce::Component
{
public:
    ModeSelector (juce::AudioProcessorValueTreeState& state, const juce::String& id,
                  const juce::StringArray& options, juce::Colour accent)
    {
        setComponentID (id + "Selector");
        const auto* namedParameter = state.getParameter (id);
        const auto parameterName = namedParameter != nullptr ? namedParameter->getName (64) : id;
        for (int i = 0; i < options.size(); ++i)
        {
            auto button = std::make_unique<juce::TextButton> (options[i]);
            button->setColour (juce::TextButton::buttonOnColourId, accent.withAlpha (0.60f));
            button->setWantsKeyboardFocus (true);
            button->setComponentID (id + "_" + juce::String (i));
            button->setTitle (parameterName + ": " + options[i]);
            juce::String help = "Choose " + options[i].toLowerCase() + " character.";
            if (id == "compType")
                help = juce::StringArray { "Soft-knee, RMS-influenced compression for gentle leveling.",
                                            "Peak-oriented compression for lively drums and rhythmic sounds.",
                                            "Smooth, linked bus compression for a cohesive mix." }[i];
            else if (id == "colorType")
                help = juce::StringArray { "Rounded tape-inspired warmth and harmonic density.",
                                            "Asymmetric tube-inspired saturation with rich even harmonics.",
                                            "Vibrant saturation with stronger presence and mixed harmonics." }[i];
            else if (id == "motionType")
                help = juce::StringArray { "Lush, gently detuned stereo chorus.",
                                            "Short modulated delays create a smooth sweeping flanger.",
                                            "Independent short voices add natural stereo thickness." }[i];
            else if (id == "clipMode")
                help = juce::StringArray { "Firmly trim peaks at the selected sample-peak ceiling.",
                                            "Round peaks with a smooth nonlinear curve." }[i];
            button->setTooltip (help);
            button->onClick = [this, i]
            {
                if (attachment != nullptr) attachment->setValueAsCompleteGesture ((float) i);
            };
            addAndMakeVisible (*button);
            buttons.push_back (std::move (button));
        }
        if (auto* parameter = state.getParameter (id))
        {
            attachment = std::make_unique<juce::ParameterAttachment> (*parameter, [this] (float value)
            {
                const auto selected = juce::roundToInt (value);
                for (int i = 0; i < (int) buttons.size(); ++i)
                    buttons[(size_t) i]->setToggleState (i == selected, juce::dontSendNotification);
            }, nullptr);
            attachment->sendInitialUpdate();
        }
    }
    void resized() override
    {
        if (buttons.empty()) return;
        const auto width = (float) getWidth() / (float) buttons.size();
        for (int i = 0; i < (int) buttons.size(); ++i)
            buttons[(size_t) i]->setBounds (juce::roundToInt ((float) i * width), 0,
                                           juce::roundToInt (width) - 3, getHeight());
    }
private:
    std::vector<std::unique_ptr<juce::TextButton>> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};
}
