#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets/FactoryPresets.h"
GummyAudioProcessor::GummyAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "GUMMY_STATE", gummy::createParameterLayout()), reader(apvts) {
    for (const auto &p : gummy::parameterSpecs())
        apvts.addParameterListener(p.id, this);
    setLatencySamples(gummy::Engine::expectedLatency);
    slots[0] = snapshot();
    slots[1] = slots[0].createCopy();
}
GummyAudioProcessor::~GummyAudioProcessor() {
    for (const auto &p : gummy::parameterSpecs())
        apvts.removeParameterListener(p.id, this);
}
void GummyAudioProcessor::parameterChanged(const juce::String &, float) {
    if (!loading.load(std::memory_order_relaxed))
        modified.store(true, std::memory_order_relaxed);
}
void GummyAudioProcessor::prepareToPlay(double sr, int n) {
    meterSampleRate = std::isfinite(sr) && sr > 0 ? sr : 48000;
    engine.prepare(sr, std::max(1, n));
    setLatencySamples(engine.latency());
}
bool GummyAudioProcessor::isBusesLayoutSupported(const BusesLayout &b) const {
    return (b.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
            b.getMainInputChannelSet() == juce::AudioChannelSet::stereo()) &&
           b.getMainInputChannelSet() == b.getMainOutputChannelSet();
}
void GummyAudioProcessor::render(juce::AudioBuffer<float> &b, bool bypassed) {
    juce::ScopedNoDenormals denormals;
    for (int ch = getTotalNumInputChannels(); ch < b.getNumChannels(); ++ch)
        b.clear(ch, 0, b.getNumSamples());
    if (b.getNumChannels() < 1 || b.getNumSamples() == 0)
        return;
    const auto meters = engine.process(b, reader.read(), bypassed);
    float meterDecay = std::exp(-float(b.getNumSamples()) / float(meterSampleRate * .3));
    auto publish = [meterDecay](std::atomic<float> &a, float v) {
        a.store(std::max(v, a.load(std::memory_order_relaxed) * meterDecay), std::memory_order_relaxed);
    };
    publish(inputMeter, meters.input);
    publish(outputMeter, meters.output);
    publish(reductionMeter, meters.reduction);
    publish(resonanceMeter, meters.resonance);
    publish(clipMeter, meters.clip);
}
void GummyAudioProcessor::processBlock(juce::AudioBuffer<float> &b, juce::MidiBuffer &) {
    render(b, false);
}
void GummyAudioProcessor::processBlockBypassed(juce::AudioBuffer<float> &b, juce::MidiBuffer &) {
    render(b, true);
}
juce::AudioProcessorParameter *GummyAudioProcessor::getBypassParameter() const {
    return apvts.getParameter("bypass");
}
juce::AudioProcessorEditor *GummyAudioProcessor::createEditor() {
    return new GummyAudioProcessorEditor(*this);
}
juce::StringArray GummyAudioProcessor::presetNames() const {
    juce::StringArray names;
    for (auto &p : gummy::factoryPresets())
        names.add(p.name);
    return names;
}
int GummyAudioProcessor::getNumPrograms() {
    return int(gummy::factoryPresets().size());
}
int GummyAudioProcessor::getCurrentProgram() {
    return std::clamp(currentPreset(), 0, getNumPrograms() - 1);
}
void GummyAudioProcessor::setCurrentProgram(int n) {
    loadFactoryPreset(n);
}
const juce::String GummyAudioProcessor::getProgramName(int n) {
    return juce::isPositiveAndBelow(n, getNumPrograms()) ? gummy::factoryPresets()[size_t(n)].name
                                                         : juce::String();
}
juce::String GummyAudioProcessor::presetDisplayName() const {
    const juce::ScopedLock guard(stateLock);
    int i = currentPreset();
    return (i >= 0 && i < int(gummy::factoryPresets().size()) ? gummy::factoryPresets()[size_t(i)].name
                                                              : userPresetName) +
           (modified.load() ? " *" : "");
}
void GummyAudioProcessor::loadFactoryPreset(int n) {
    if (!juce::isPositiveAndBelow(n, getNumPrograms()))
        return;
    loading.store(true);
    for (const auto &spec : gummy::parameterSpecs()) {
        auto *p = apvts.getParameter(spec.id);
        float v = spec.initial;
        for (const auto &item : gummy::factoryPresets()[size_t(n)].values)
            if (item.first == spec.id)
                v = item.second;
        p->setValueNotifyingHost(p->convertTo0to1(v));
    }
    presetIndex.store(n);
    modified.store(false);
    loading.store(false);
}
juce::ValueTree GummyAudioProcessor::snapshot() {
    const juce::ScopedLock guard(stateLock);
    auto tree = apvts.copyState();
    tree.setProperty("schemaVersion", 1, nullptr);
    tree.setProperty("userPresetName", userPresetName, nullptr);
    tree.setProperty("presetIndex", currentPreset(), nullptr);
    tree.setProperty("modified", modified.load(), nullptr);
    return tree;
}
bool GummyAudioProcessor::restore(const juce::ValueTree &tree) {
    const juce::ScopedLock guard(stateLock);
    if (!tree.hasType("GUMMY_STATE") || int(tree.getProperty("schemaVersion", 1)) != 1)
        return false;
    // Require every known parameter exactly once and reject non-finite/out-of-range data.
    for (const auto &spec : gummy::parameterSpecs()) {
        int count = 0;
        for (const auto &child : tree)
            if (child.getProperty("id").toString() == spec.id) {
                double v = double(child.getProperty("value"));
                if (!std::isfinite(v) || v < spec.lo - .001 || v > spec.hi + .001)
                    return false;
                ++count;
            }
        if (count != 1)
            return false;
    }
    loading.store(true);
    userPresetName = tree.getProperty("userPresetName", "USER PRESET").toString();
    apvts.replaceState(tree.createCopy());
    int restoredIndex = int(tree.getProperty("presetIndex", -1));
    presetIndex.store(restoredIndex >= 0 && restoredIndex < getNumPrograms() ? restoredIndex : -1);
    modified.store(bool(tree.getProperty("modified", false)));
    loading.store(false);
    return true;
}
void GummyAudioProcessor::getStateInformation(juce::MemoryBlock &dest) {
    const juce::ScopedLock guard(stateLock);
    auto tree = snapshot();
    tree.setProperty("slotB", slotB.load(), nullptr);
    for (int i = 0; i < 2; ++i) {
        juce::ValueTree wrap(i == 0 ? "SLOT_A" : "SLOT_B");
        wrap.addChild(slots[i].createCopy(), -1, nullptr);
        tree.addChild(wrap, -1, nullptr);
    }
    if (auto xml = tree.createXml())
        copyXmlToBinary(*xml, dest);
}
void GummyAudioProcessor::setStateInformation(const void *data, int size) {
    const juce::ScopedLock guard(stateLock);
    if (auto xml = getXmlFromBinary(data, size)) {
        auto tree = juce::ValueTree::fromXml(*xml);
        auto a = tree.getChildWithName("SLOT_A").getChild(0).createCopy(),
             b = tree.getChildWithName("SLOT_B").getChild(0).createCopy();
        bool selected = bool(tree.getProperty("slotB", false));
        tree.removeChild(tree.getChildWithName("SLOT_A"), nullptr);
        tree.removeChild(tree.getChildWithName("SLOT_B"), nullptr);
        if (restore(tree)) {
            slots[0] = a.isValid() ? a : snapshot();
            slots[1] = b.isValid() ? b : snapshot();
            slotB.store(selected);
        }
    }
}
void GummyAudioProcessor::switchAB() {
    const juce::ScopedLock guard(stateLock);
    int old = slotB.load() ? 1 : 0;
    slots[old] = snapshot();
    int next = 1 - old;
    if (restore(slots[next]))
        slotB.store(next == 1);
}
bool GummyAudioProcessor::saveUserPreset(const juce::File &file) {
    const juce::ScopedLock guard(stateLock);
    auto tree = snapshot();
    tree.setProperty("presetIndex", -1, nullptr);
    tree.setProperty("userPresetName", file.getFileNameWithoutExtension(), nullptr);
    tree.setProperty("modified", false, nullptr);
    auto xml = tree.createXml();
    if (!xml || !xml->writeTo(file))
        return false;
    userPresetName = file.getFileNameWithoutExtension();
    presetIndex.store(-1);
    modified.store(false);
    return true;
}
bool GummyAudioProcessor::loadUserPreset(const juce::File &file) {
    auto xml = juce::XmlDocument::parse(file);
    if (!xml)
        return false;
    auto tree = juce::ValueTree::fromXml(*xml);
    tree.setProperty("presetIndex", -1, nullptr);
    tree.setProperty("userPresetName", file.getFileNameWithoutExtension(), nullptr);
    tree.setProperty("modified", false, nullptr);
    return restore(tree);
}
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
    return new GummyAudioProcessor;
}
