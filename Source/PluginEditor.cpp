#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace gummy::ui;

GummyAudioProcessorEditor::GummyAudioProcessorEditor (GummyAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p), tooltip (this, 650)
{
    setLookAndFeel (&lookAndFeel);
    setOpaque (true);
    setTitle ("GUMMY — Sweeten your sound");
    addAndMakeVisible (canvas);
    auto& state = processor.apvts;
    transientPanel = std::make_unique<ModulePanel> (state, "transientOn", "01", "TRANSIENT", "SHAPE THE FEEL", pink());
    compressPanel  = std::make_unique<ModulePanel> (state, "compOn", "02", "COMPRESS", "MUSICAL DYNAMICS", lavender());
    colorPanel     = std::make_unique<ModulePanel> (state, "colorOn", "03", "COLOR", "A LITTLE HARMONIC MAGIC", peach());
    tonePanel      = std::make_unique<ModulePanel> (state, "toneOn", "04", "TONE", "THREE KNOBS. YOUR SIGNATURE.", blue());
    smoothPanel    = std::make_unique<ModulePanel> (state, "smoothOn", "05", "SMOOTH", "TAME THE SHARP EDGES", mint());
    motionPanel    = std::make_unique<ModulePanel> (state, "motionOn", "06", "MOTION", "MAKE A LITTLE MOVEMENT", lavender());
    clipPanel      = std::make_unique<ModulePanel> (state, "clipOn", "07", "CLIP", "THE FINISHING TOUCH", pink());
    for (auto* panel : { transientPanel.get(), compressPanel.get(), colorPanel.get(), tonePanel.get(),
                         smoothPanel.get(), motionPanel.get(), clipPanel.get() })
        canvas.addAndMakeVisible (*panel);

    addKnob (*transientPanel, "attack", "ATTACK", "Add impact to transients, or soften their initial attack.", pink(), true);
    addKnob (*transientPanel, "sustain", "SUSTAIN", "Lengthen or tighten the body and decay of a sound.", pink(), true);
    addKnob (*compressPanel, "threshold", "THRESHOLD", "Compression begins as the signal rises above this level.", lavender());
    addKnob (*compressPanel, "ratio", "RATIO", "Set how firmly the compressor controls peaks above the threshold.", lavender());
    addKnob (*compressPanel, "compAttack", "ATTACK", "Set how quickly compression responds to incoming peaks.", lavender());
    addKnob (*compressPanel, "release", "RELEASE", "Set how quickly gain recovers. Auto Release adds program-dependent recovery.", lavender());
    addKnob (*compressPanel, "makeup", "MAKEUP", "Restore level after compression.", lavender(), true);
    addKnob (*compressPanel, "compMix", "MIX", "Blend the compressed sound with its aligned dry signal.", lavender());
    addKnob (*colorPanel, "colorDrive", "DRIVE", "Add warm, compensated harmonic saturation.", peach());
    addKnob (*colorPanel, "colorMix", "MIX", "Blend harmonic color into the original sound.", peach());
    addKnob (*tonePanel, "body", "BODY", "Add bass weight and low-mid warmth, or clear away thickness.", blue(), true);
    addKnob (*tonePanel, "presence", "PRESENCE", "Bring articulation forward, or soften the upper midrange.", blue(), true);
    addKnob (*tonePanel, "air", "AIR", "Add glossy openness, or gently soften the high frequencies.", blue(), true);
    addKnob (*smoothPanel, "smooth", "AMOUNT", "Dynamically reduce narrow resonances while preserving the surrounding tone.", mint());
    addKnob (*motionPanel, "motionAmount", "AMOUNT", "Add stereo movement and depth. Higher settings increase intensity and wet contribution.", lavender());
    addKnob (*clipPanel, "clipDrive", "DRIVE", "Push the signal into the clipper to shape and control peaks.", pink());
    addKnob (*clipPanel, "ceiling", "CEILING", "Set the clipper's sample-peak ceiling. Output gain and dry blending can exceed it.", pink());
    addKnob (canvas, "input", "INPUT", "Adjust level before the processing chain.", blue(), true);
    addKnob (canvas, "output", "OUTPUT", "Adjust final output level. Positive gain can exceed the clipper ceiling.", pink(), true);
    addKnob (canvas, "mix", "GLOBAL MIX", "Blend the entire processed chain with its latency-aligned dry signal.", lavender());
    for (size_t i = 17; i < knobs.size(); ++i)
        knobs[i]->slider.setSliderStyle (juce::Slider::LinearHorizontal);

    compModes = std::make_unique<ModeSelector> (state, "compType", juce::StringArray { "SMOOTH", "PUNCH", "GLUE" }, lavender());
    colorModes = std::make_unique<ModeSelector> (state, "colorType", juce::StringArray { "TAPE", "TUBE", "COLOR" }, peach());
    motionModes = std::make_unique<ModeSelector> (state, "motionType", juce::StringArray { "CHORUS", "FLANGER", "DOUBLER" }, lavender());
    clipModes = std::make_unique<ModeSelector> (state, "clipMode", juce::StringArray { "HARD", "SOFT" }, pink());
    compressPanel->addAndMakeVisible (*compModes);
    colorPanel->addAndMakeVisible (*colorModes);
    motionPanel->addAndMakeVisible (*motionModes);
    clipPanel->addAndMakeVisible (*clipModes);
    compressPanel->addAndMakeVisible (autoRelease);
    autoRelease.setClickingTogglesState (true);
    autoRelease.setComponentID ("autoRelease");
    autoRelease.setColour (juce::TextButton::buttonOnColourId, lavender().withAlpha (0.6f));
    autoRelease.setTooltip ("Use a program-dependent release envelope for smooth recovery.");
    autoReleaseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "autoRelease", autoRelease);
    compressPanel->addAndMakeVisible (reductionMeter);
    smoothPanel->addAndMakeVisible (resonanceMeter);
    clipPanel->addAndMakeVisible (clippingMeter);
    canvas.addAndMakeVisible (inputMeter);
    canvas.addAndMakeVisible (outputMeter);

    for (auto* button : { &previous, &next, &save, &load, &comparison, &bypass })
    {
        canvas.addAndMakeVisible (*button);
        button->setColour (juce::TextButton::buttonOnColourId, pink().withAlpha (0.65f));
        button->setWantsKeyboardFocus (true);
    }
    previous.setTooltip ("Load the previous factory preset.");
    next.setTooltip ("Load the next factory preset.");
    save.setTooltip ("Save the current settings to a .gummy preset file.");
    load.setTooltip ("Load a .gummy user preset file.");
    comparison.setTooltip ("Switch between independent A and B settings for comparison.");
    bypass.setTooltip ("Bypass the full effect with a smooth, latency-aligned transition.");
    previous.setTitle ("Previous factory preset");
    next.setTitle ("Next factory preset");
    previous.onClick = [this] { stepPreset (-1); };
    next.onClick = [this] { stepPreset (1); };
    save.onClick = [this] { choosePresetFile (true); };
    load.onClick = [this] { choosePresetFile (false); };
    comparison.onClick = [this] { processor.switchAB(); };
    bypass.setClickingTogglesState (true);
    bypass.setComponentID ("bypass");
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, "bypass", bypass);
    canvas.addAndMakeVisible (presetSelector);
    presetSelector.setTitle ("Factory preset");
    presetSelector.setTooltip ("Choose a factory starting point. An asterisk marks modified settings.");
    const auto names = processor.presetNames();
    for (int i = 0; i < names.size(); ++i) presetSelector.addItem (names[i], i + 1);
    presetSelector.onChange = [this]
    {
        const auto index = presetSelector.getSelectedId() - 1;
        if (index >= 0) processor.loadFactoryPreset (index);
    };
    canvas.addAndMakeVisible (qualitySelector);
    qualitySelector.addItemList ({ "OFF", "2x", "4x" }, 1);
    qualitySelector.setTitle ("Oversampling quality");
    qualitySelector.setComponentID ("quality");
    qualitySelector.setTooltip ("Oversample nonlinear stages to reduce aliasing. Higher quality uses more CPU.");
    qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, "quality", qualitySelector);
    canvas.addAndMakeVisible (status);
    status.setFont (font (10.0f));
    status.setColour (juce::Label::textColourId, muted());
    status.setJustificationType (juce::Justification::centredRight);
    status.setInterceptsMouseClicks (false, false);
    status.setText ("NATIVE AUDIO / INFINITE POSSIBILITIES", juce::dontSendNotification);
    setResizable (true, true);
    setResizeLimits (880, 576, 1650, 1080);
    if (auto* boundsConstrainer = getConstrainer()) boundsConstrainer->setFixedAspectRatio (1100.0 / 720.0);
    setSize (1100, 720);
    timerCallback();
    startTimerHz (30);
}

GummyAudioProcessorEditor::~GummyAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

GummyKnob& GummyAudioProcessorEditor::addKnob (juce::Component& parent, const char* id, const char* label,
                                              const char* help, juce::Colour accent, bool bipolar)
{
    auto knob = std::make_unique<GummyKnob> (processor.apvts, id, label, help, accent, bipolar);
    parent.addAndMakeVisible (*knob);
    // Rebuild the cached value label now that the custom look-and-feel is
    // inherited from the editor, rather than from JUCE's default theme.
    knob->slider.lookAndFeelChanged();
    knobs.push_back (std::move (knob));
    return *knobs.back();
}

void GummyAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xffeae6f0));
    const float scale = juce::jmin ((float) getWidth() / 1100.0f, (float) getHeight() / 720.0f);
    g.addTransform (juce::AffineTransform::scale (scale).translated (((float) getWidth() - 1100.0f * scale) * 0.5f,
                                                                  ((float) getHeight() - 720.0f * scale) * 0.5f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffaf9fc), 100, 0, juce::Colour (0xffe6e0ee), 1080, 720, false));
    g.fillRect (0, 0, 1100, 720);
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawRoundedRectangle (juce::Rectangle<float> (9, 9, 1082, 702), 25.0f, 1.0f);

    // A restrained pearlescent header, drawn as vectors for high-DPI hosts.
    g.setColour (lavender().withAlpha (0.10f));
    g.fillEllipse (-70, -110, 410, 220);
    g.setColour (pink().withAlpha (0.08f));
    g.fillEllipse (100, -130, 360, 220);
    g.setColour (ink());
    g.setFont (font (45.0f, true));
    g.drawText ("GUMMY", 27, 18, 245, 58, juce::Justification::centredLeft);
    g.setFont (font (10.5f, true));
    g.setColour (muted());
    g.drawText ("S W E E T E N   Y O U R   S O U N D", 30, 75, 300, 18, juce::Justification::centredLeft);
    // A little four-point acrylic star is the only decorative mark.
    juce::Path sparkle;
    sparkle.startNewSubPath (256, 30); sparkle.lineTo (259, 39); sparkle.lineTo (268, 42);
    sparkle.lineTo (259, 45); sparkle.lineTo (256, 54); sparkle.lineTo (253, 45);
    sparkle.lineTo (244, 42); sparkle.lineTo (253, 39); sparkle.closeSubPath();
    g.setColour (pink().withAlpha (0.65f));
    g.fillPath (sparkle);

    g.setColour (muted());
    g.setFont (font (9.0f, true));
    g.drawText ("YOUR NEXT FAVORITE SOUND", 376, 24, 342, 13, juce::Justification::centredLeft);
    g.drawText ("COMPARE", 879, 24, 77, 13, juce::Justification::centred);
    g.drawText ("MASTER", 971, 24, 104, 13, juce::Justification::centred);
    g.setColour (juce::Colour (0xffd7d0e2));
    g.drawHorizontalLine (111, 25.0f, 1075.0f);
    g.setFont (font (9.5f, true));
    g.setColour (muted());
    g.drawText ("01  SHAPE", 28, 119, 120, 15, juce::Justification::centredLeft);
    g.drawText ("02  POLISH", 249, 119, 120, 15, juce::Justification::centredLeft);
    g.drawText ("03  SWEETEN", 710, 119, 160, 15, juce::Justification::centredLeft);
    g.drawText ("A COMPLETE CHAIN. ONE SWEET SPOT.", 777, 119, 295, 15, juce::Justification::centredRight);

    card (g, juce::Rectangle<float> (24, 632, 1052, 66), 18.0f);
    g.setColour (juce::Colour (0xffdcd6e6));
    g.drawVerticalLine (373, 645.0f, 685.0f);
    g.drawVerticalLine (760, 645.0f, 685.0f);
    g.setColour (muted());
    g.setFont (font (9.0f, true));
    g.drawText ("OVERSAMPLING", 784, 644, 117, 13, juce::Justification::centredLeft);
    g.setFont (font (10.0f, true));
    g.drawText ("SWEET BY DESIGN.", 923, 650, 130, 16, juce::Justification::centredRight);
    g.setFont (font (8.5f));
    g.drawText ("GUMMY AUDIO   /   1.0", 923, 670, 130, 12, juce::Justification::centredRight);
}

void GummyAudioProcessorEditor::resized()
{
    const auto scale = juce::jmin ((float) getWidth() / 1100.0f, (float) getHeight() / 720.0f);
    canvas.setBounds (0, 0, 1100, 720);
    canvas.setTransform (juce::AffineTransform::scale (scale).translated (((float) getWidth() - 1100.0f * scale) * 0.5f,
                                                                       ((float) getHeight() - 720.0f * scale) * 0.5f));
    transientPanel->setBounds (24, 145, 206, 231);
    compressPanel->setBounds (242, 145, 448, 231);
    colorPanel->setBounds (702, 145, 374, 231);
    tonePanel->setBounds (24, 389, 406, 230);
    smoothPanel->setBounds (442, 389, 188, 230);
    motionPanel->setBounds (642, 389, 216, 230);
    clipPanel->setBounds (870, 389, 206, 230);
    knobs[0]->setBounds (6, 62, 97, 141);
    knobs[1]->setBounds (103, 62, 97, 141);
    for (int i = 0; i < 6; ++i) knobs[(size_t) i + 2]->setBounds (13 + i * 70, 87, 70, 105);
    compModes->setBounds (15, 47, 255, 27);
    autoRelease.setBounds (285, 47, 147, 27);
    reductionMeter.setBounds (180, 200, 250, 23);
    colorModes->setBounds (40, 47, 294, 27);
    knobs[8]->setBounds (57, 84, 110, 117);
    knobs[9]->setBounds (209, 84, 110, 117);
    for (int i = 0; i < 3; ++i) knobs[(size_t) i + 10]->setBounds (15 + i * 126, 56, 126, 149);
    knobs[13]->setBounds (33, 48, 122, 132);
    resonanceMeter.setBounds (16, 184, 156, 22);
    motionModes->setBounds (13, 47, 193, 26);
    knobs[14]->setBounds (48, 82, 120, 122);
    clipModes->setBounds (15, 47, 177, 26);
    knobs[15]->setBounds (7, 82, 96, 99);
    knobs[16]->setBounds (103, 82, 96, 99);
    clippingMeter.setBounds (16, 184, 174, 22);
    // Compact global controls use the same attached rotary widgets.
    knobs[17]->setBounds (39, 636, 94, 59);
    knobs[18]->setBounds (149, 636, 94, 59);
    knobs[19]->setBounds (259, 636, 94, 59);
    inputMeter.setBounds (397, 641, 334, 24);
    outputMeter.setBounds (397, 668, 334, 24);
    qualitySelector.setBounds (784, 661, 117, 25);
    presetSelector.setBounds (411, 43, 307, 34);
    previous.setBounds (376, 43, 28, 34);
    next.setBounds (725, 43, 28, 34);
    save.setBounds (767, 43, 45, 34);
    load.setBounds (819, 43, 45, 34);
    comparison.setBounds (879, 43, 77, 34);
    bypass.setBounds (970, 43, 105, 34);
    status.setBounds (376, 81, 698, 18);
}

void GummyAudioProcessorEditor::timerCallback()
{
    // Consume the published peaks so meters decay when a host suspends audio.
    inputMeter.setReading (processor.inputMeter.exchange (0.0f, std::memory_order_relaxed));
    outputMeter.setReading (processor.outputMeter.exchange (0.0f, std::memory_order_relaxed));
    reductionMeter.setReading (processor.reductionMeter.exchange (0.0f, std::memory_order_relaxed));
    resonanceMeter.setReading (processor.resonanceMeter.exchange (0.0f, std::memory_order_relaxed));
    clippingMeter.setReading (processor.clipMeter.exchange (0.0f, std::memory_order_relaxed));
    const auto name = processor.presetDisplayName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        presetSelector.setText (name, juce::dontSendNotification);
    }
    comparison.setButtonText (processor.isSlotB() ? "A / [ B ]" : "[ A ] / B");
    bypass.setButtonText (bypass.getToggleState() ? "BYPASSED" : "BYPASS");
    if (statusTicks > 0 && --statusTicks == 0)
        status.setText ("NATIVE AUDIO / INFINITE POSSIBILITIES", juce::dontSendNotification);
}

void GummyAudioProcessorEditor::stepPreset (int direction)
{
    const auto count = processor.presetNames().size();
    if (count == 0) return;
    processor.loadFactoryPreset ((juce::jmax (0, processor.currentPreset()) + count + direction) % count);
}

void GummyAudioProcessorEditor::showStatus (const juce::String& text)
{
    status.setText (text, juce::dontSendNotification);
    statusTicks = 150;
}

void GummyAudioProcessorEditor::choosePresetFile (bool saving)
{
    const auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    fileChooser = std::make_unique<juce::FileChooser> (saving ? "Save GUMMY preset" : "Load GUMMY preset",
                                                    saving ? folder.getChildFile ("My Sweet Sound.gummy") : folder,
                                                    "*.gummy");
    const auto chooserFlags = saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::warnAboutOverwriting
                              : juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    auto safe = juce::Component::SafePointer<GummyAudioProcessorEditor> (this);
    fileChooser->launchAsync (chooserFlags, [safe, saving] (const juce::FileChooser& chooser)
    {
        if (safe == nullptr) return;
        auto file = chooser.getResult();
        if (file == juce::File()) return;
        if (saving) file = file.withFileExtension ("gummy");
        const auto success = saving ? safe->processor.saveUserPreset (file) : safe->processor.loadUserPreset (file);
        safe->showStatus (success ? (saving ? "PRESET SAVED: " : "PRESET LOADED: ") + file.getFileNameWithoutExtension()
                                  : "PRESET ERROR: Check that the file is a valid GUMMY preset and the folder is writable.");
    });
}
