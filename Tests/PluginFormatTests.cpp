#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

// Load the actual built VST3 through JUCE's host API. This exercises the binary
// entry point, processor/controller wrapper, bus negotiation and state bridge,
// rather than linking the processor implementation into this executable.
namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
std::vector<float> values(juce::AudioProcessor& p) {
    std::vector<float> result;
    for (auto* parameter : p.getParameters()) result.push_back(parameter->getValue());
    return result;
}
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    if (argc != 2) {
        std::cerr << "Usage: GummyFormatTests /absolute/path/GUMMY.vst3\n";
        return 2;
    }
    try {
        const auto bundle = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
        require(bundle.exists(), "VST3 bundle does not exist");
        juce::VST3PluginFormat format;
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile(descriptions, bundle.getFullPathName());
        require(descriptions.size() == 1, "Expected one discoverable VST3 effect");
        require(descriptions[0]->name == "GUMMY", "VST3 name is incorrect");
        require(!descriptions[0]->isInstrument, "VST3 is incorrectly classified as an instrument");
        std::cout << "PASS VST3 discovery: " << descriptions[0]->name << " / "
                  << descriptions[0]->manufacturerName << " / " << descriptions[0]->version << '\n';

        juce::String error;
        auto plugin = format.createInstanceFromDescription(*descriptions[0], 48000, 128, error);
        if (plugin == nullptr) throw std::runtime_error("VST3 instantiation failed: " + error.toStdString());
        require(plugin->getParameters().size() >= 34, "VST3 does not expose the full parameter set");
        require(plugin->hasEditor(), "VST3 does not expose a custom editor");
        plugin->setRateAndBufferSizeDetails(48000, 128);
        plugin->prepareToPlay(48000, 128);
        std::cout << "INFO VST3 reported latency after prepare: " << plugin->getLatencySamples()
                  << " samples at 48 kHz\n";
        juce::AudioBuffer<float> flush(2, 128); juce::MidiBuffer flushMidi;
        auto flushParameters = [&] { flush.clear(); plugin->processBlock(flush, flushMidi); };
        const auto& parameters = plugin->getParameters();
        for (int i = 0; i < parameters.size(); ++i) {
            if (parameters[i]->getName(100) == "Program") continue;
            float value = static_cast<float>((i * 31 + 19) % 81 + 10) / 100.0f;
            if (parameters[i]->isDiscrete()) {
                const int steps = parameters[i]->getNumSteps();
                if (steps > 1 && steps < 1000000)
                    value = std::round(value * static_cast<float>(steps - 1)) / static_cast<float>(steps - 1);
            }
            parameters[i]->setValueNotifyingHost(value);
        }
        flushParameters(); // Deliver host automation queues to the VST3 component.
        const auto expected = values(*plugin);
        juce::MemoryBlock state; plugin->getStateInformation(state);
        require(state.getSize() > 100, "VST3 session state is empty");
        for (auto* parameter : parameters)
            if (parameter->getName(100) != "Program") parameter->setValueNotifyingHost(0);
        flushParameters();
        plugin->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        flushParameters();
        const auto restored = values(*plugin);
        require(expected.size() == restored.size(), "VST3 parameter count changed during restore");
        bool roundTripOK = true;
        for (std::size_t i = 0; i < expected.size(); ++i)
            if (std::abs(expected[i] - restored[i]) >= 0.0001f) {
                roundTripOK = false;
                std::cerr << "INFO VST3 restore mismatch " << i << " " << parameters[static_cast<int>(i)]->getName(100)
                          << ": expected " << expected[i] << ", restored " << restored[i] << '\n';
            }
        require(roundTripOK, "VST3 state did not restore parameter values");
        std::cout << "PASS VST3 parameter automation and state bridge (" << parameters.size() << " parameters)\n";
        plugin->releaseResources();

        constexpr std::array<int, 12> sizes { 1, 2, 3, 7, 15, 31, 64, 127, 129, 257, 509, 2053 };
        std::size_t callbacks = 0;
        for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
            for (int channels : { 1, 2 }) {
                const auto set = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
                juce::AudioProcessor::BusesLayout layout;
                layout.inputBuses.add(set); layout.outputBuses.add(set);
                require(plugin->setBusesLayout(layout), "VST3 mono/stereo bus negotiation failed");
                // The VST3 host must respect ProcessSetup::maxSamplesPerBlock:
                // JUCE's host wrapper owns scratch buffers sized to this maximum.
                // ProcessorTests separately sends blocks above its prepare hint
                // to verify GUMMY's defensive internal chunking.
                plugin->setRateAndBufferSizeDetails(rate, 2053);
                plugin->prepareToPlay(rate, 2053);
                const int latency = plugin->getLatencySamples();
                require(latency >= 0 && latency < 4096, "VST3 reports invalid latency");
                juce::AudioBuffer<float> b(channels, 2053); juce::MidiBuffer midi;
                int position = 0;
                for (int cycle = 0; cycle < 3; ++cycle)
                    for (int n : sizes) {
                        b.setSize(channels, n, false, false, true);
                        for (int c = 0; c < channels; ++c)
                            for (int i = 0; i < n; ++i)
                                b.setSample(c, i, static_cast<float>(0.2 * std::sin(6.283185307179586 * (443 + c * 71) * (position + i) / rate)));
                        plugin->processBlock(b, midi); ++callbacks;
                        for (int c = 0; c < channels; ++c)
                            for (int i = 0; i < n; ++i)
                                require(std::isfinite(b.getSample(c, i)), "VST3 produced non-finite audio");
                        require(plugin->getLatencySamples() == latency, "VST3 latency changes unexpectedly");
                        position += n;
                    }
                plugin->releaseResources();
            }
        std::cout << "PASS VST3 mono/stereo, six sample rates, irregular 1..2053 sample blocks within declared host maximum (" << callbacks << " callbacks)\n";
        auto second = format.createInstanceFromDescription(*descriptions[0], 48000, 128, error);
        require(second != nullptr, "Cannot load a second VST3 instance");
        second->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        const auto secondValues = values(*second);
        require(secondValues.size() == expected.size(), "Second VST3 instance parameter mismatch");
        for (std::size_t i = 0; i < expected.size(); ++i)
            require(std::abs(secondValues[i] - expected[i]) < 0.0001f, "Fresh VST3 instance state restore failed");
        std::cout << "PASS independent second VST3 instance and state restoration\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL VST3 integration: " << e.what() << '\n';
        return 1;
    }
}
