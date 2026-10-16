#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// These replacement allocation functions belong only to this test executable.
// A thread-local gate counts C++ allocations during processBlock, excluding test
// buffers, host parameter notification, GUI rendering, and initial preparation.
namespace rt_allocations {
thread_local bool enabled = false;
thread_local std::size_t count = 0;
void note() noexcept { if (enabled) ++count; }
void* allocate(std::size_t size) {
    note();
    if (auto* p = std::malloc(std::max<std::size_t>(size, 1))) return p;
    throw std::bad_alloc();
}
void* alignedAllocate(std::size_t size, std::size_t alignment) {
    note();
   #if defined(_MSC_VER)
    if (auto* p = _aligned_malloc(std::max<std::size_t>(size, 1), alignment)) return p;
   #else
    void* result = nullptr;
    if (posix_memalign(&result, alignment, std::max<std::size_t>(size, 1)) == 0) return result;
   #endif
    throw std::bad_alloc();
}
void alignedFree(void* p) noexcept {
   #if defined(_MSC_VER)
    _aligned_free(p);
   #else
    std::free(p);
   #endif
}
}
void* operator new(std::size_t n) { return rt_allocations::allocate(n); }
void* operator new[](std::size_t n) { return rt_allocations::allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n, std::align_val_t a) { return rt_allocations::alignedAllocate(n, static_cast<std::size_t>(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return rt_allocations::alignedAllocate(n, static_cast<std::size_t>(a)); }
void operator delete(void* p, std::align_val_t) noexcept { rt_allocations::alignedFree(p); }
void operator delete[](void* p, std::align_val_t) noexcept { rt_allocations::alignedFree(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { rt_allocations::alignedFree(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { rt_allocations::alignedFree(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { try { return ::operator new[](n); } catch (...) { return nullptr; } }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr std::array<int, 14> blockSizes { 1, 2, 3, 5, 7, 15, 31, 64, 127, 129, 257, 509, 1024, 2053 };
int passed = 0, failed = 0;
std::size_t callbacks = 0;

void require(bool condition, const std::string& description) {
    if (!condition) throw std::runtime_error(description);
}
void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}

template<typename Fn> void test(const char* name, Fn&& fn) {
    try { fn(); ++passed; std::cout << "PASS " << name << '\n'; }
    catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
}

void set(GummyAudioProcessor& p, const char* id, float value) {
    auto* parameter = p.apvts.getParameter(id);
    require(parameter != nullptr, std::string("Missing parameter ") + id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float actual(GummyAudioProcessor& p, const char* id) {
    auto* value = p.apvts.getRawParameterValue(id);
    require(value != nullptr, std::string("Missing parameter ") + id);
    return value->load();
}

void prepare(GummyAudioProcessor& p, int channels, double rate, int block = 128) {
    juce::AudioProcessor::BusesLayout layout;
    const auto channelSet = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    require(p.isBusesLayoutSupported(layout), "Requested mono/stereo layout rejected");
    require(p.setBusesLayout(layout), "Could not apply layout");
    p.setRateAndBufferSizeDetails(rate, block);
    p.prepareToPlay(rate, block);
}

void process(GummyAudioProcessor& p, juce::AudioBuffer<float>& buffer, bool allocationCheck = true) {
    juce::MidiBuffer midi;
    const auto before = rt_allocations::count;
    rt_allocations::enabled = allocationCheck;
    try { p.processBlock(buffer, midi); }
    catch (...) { rt_allocations::enabled = false; throw; }
    rt_allocations::enabled = false;
    ++callbacks;
    require(!allocationCheck || before == rt_allocations::count, "C++ heap allocation inside processBlock");
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            require(std::isfinite(buffer.getSample(c, i)), "Non-finite audio output");
}

float signal(int channel, int position, double rate) {
    const auto t = static_cast<double>(position) / rate;
    return static_cast<float>(0.12 * std::sin(2.0 * pi * (337.0 + channel * 83.0) * t)
                            + 0.06 * std::sin(2.0 * pi * 3711.0 * t + channel * 0.3));
}

void settle(GummyAudioProcessor& p, int channels, int samples = 16384) {
    juce::AudioBuffer<float> b(channels, 257);
    while (samples > 0) {
        const int n = std::min(samples, 257);
        b.setSize(channels, n, false, false, true);
        b.clear(); process(p, b); samples -= n;
    }
}

// Compare the complete stream with the reference, across deliberately irregular
// host blocks. This catches a one-sample error at every block boundary as well
// as an incorrect host latency value or unaligned dry signal.
float alignedError(GummyAudioProcessor& p, int channels, double rate, int samples = 17000) {
    const int latency = p.getLatencySamples();
    require(latency >= 0 && latency < 4096, "Implausible reported latency");
    juce::AudioBuffer<float> b(channels, 2053);
    float error = 0;
    int offset = 0, block = 0;
    while (offset < samples) {
        const int n = std::min(samples - offset, blockSizes[static_cast<std::size_t>(block++) % blockSizes.size()]);
        b.setSize(channels, n, false, false, true);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < n; ++i) b.setSample(c, i, signal(c, offset + i, rate));
        process(p, b);
        for (int c = 0; c < channels; ++c)
            for (int i = 0; i < n; ++i) {
                const int reference = offset + i - latency;
                const float expected = reference < 0 ? 0.0f : signal(c, reference, rate);
                error = std::max(error, std::abs(b.getSample(c, i) - expected));
            }
        offset += n;
    }
    return error;
}

void neutralAndLatency() {
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        for (int channels : { 1, 2 }) {
            GummyAudioProcessor p;
            p.loadFactoryPreset(0); set(p, "quality", 0);
            prepare(p, channels, rate, 32);
            require(alignedError(p, channels, rate) < 0.00005f, "Neutral Off mode is not unity at reported latency");
            p.releaseResources();
        }
}

void dryAndBypass() {
    int fixedLatency = -1;
    for (int quality : { 0, 1, 2 })
        for (int route : { 0, 1, 2 }) {
            GummyAudioProcessor p;
            p.loadFactoryPreset(9); set(p, "quality", static_cast<float>(quality));
            set(p, "input", 14); set(p, "output", 0);
            if (route == 0) set(p, "mix", 0);
            if (route == 1) set(p, "bypass", 1);
            prepare(p, 2, 48000, 64);
            if (fixedLatency < 0) fixedLatency = p.getLatencySamples();
            require(p.getLatencySamples() == fixedLatency, "Quality selection changes fixed host latency");
            if (route < 2) require(alignedError(p, 2, 48000) < 0.00005f, "Dry/global bypass path differs from latency-aligned input");
            else {
                // JUCE hosts may call processBlockBypassed instead of automating
                // the plug-in's own bypass parameter.
                const int latency = p.getLatencySamples();
                juce::AudioBuffer<float> b(2, 2053); juce::MidiBuffer midi;
                int offset = 0;
                for (int size : blockSizes) {
                    b.setSize(2, size, false, false, true);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < size; ++i) b.setSample(c, i, signal(c, offset + i, 48000));
                    const auto before = rt_allocations::count;
                    rt_allocations::enabled = true; p.processBlockBypassed(b, midi); rt_allocations::enabled = false;
                    require(before == rt_allocations::count, "Allocation in host bypass callback");
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < size; ++i) {
                            const int position = offset + i - latency;
                            const float expected = position < 0 ? 0.0f : signal(c, position, 48000);
                            require(std::abs(b.getSample(c, i) - expected) < 0.00005f, "Host bypass latency differs from processing latency");
                        }
                    offset += size;
                }
            }
        }
}

void automateAndReprepare() {
    GummyAudioProcessor p;
    std::uint32_t random = 0x71b4ca93u;
    auto next = [&] { random ^= random << 13; random ^= random >> 17; random ^= random << 5; return random; };
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 })
        for (int channels : { 1, 2 }) {
            prepare(p, channels, rate, 31);
            juce::AudioBuffer<float> empty(channels, 0); process(p, empty);
            const int latency = p.getLatencySamples();
            juce::AudioBuffer<float> b(channels, 2053);
            for (int block = 0; block < 125; ++block) {
                const auto& parameters = p.getParameters();
                for (int change = 0; change < 5; ++change) {
                    auto* parameter = parameters[static_cast<int>(next() % static_cast<unsigned>(parameters.size()))];
                    parameter->setValueNotifyingHost(static_cast<float>(next() % 10001u) / 10000.0f);
                }
                set(p, "quality", static_cast<float>(block % 3));
                set(p, "bypass", static_cast<float>((block / 3) % 2));
                const int n = blockSizes[static_cast<std::size_t>(block) % blockSizes.size()];
                b.setSize(channels, n, false, false, true);
                for (int c = 0; c < channels; ++c)
                    for (int i = 0; i < n; ++i)
                        b.setSample(c, i, (block % 11 == 0 ? 20.0f : 1.0f) * signal(c, block * 2053 + i, rate));
                process(p, b);
                require(p.getLatencySamples() == latency, "Automation changes fixed latency");
            }
            p.releaseResources();
        }
}

void bypassTransition() {
    GummyAudioProcessor p; p.loadFactoryPreset(0);
    set(p, "quality", 0); set(p, "input", 12); prepare(p, 2, 48000);
    juce::AudioBuffer<float> buffer(2, 127);
    auto constantBlock = [&] {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < buffer.getNumSamples(); ++i) buffer.setSample(c, i, 0.1f);
        process(p, buffer);
    };
    for (int i = 0; i < 50; ++i) constantBlock();
    float previous = buffer.getSample(0, buffer.getNumSamples() - 1);
    require(std::abs(previous - 0.1f * std::pow(10.0f, 12.0f / 20.0f)) < 0.00005f, "Test gain did not settle");
    const int latency = p.getLatencySamples();
    buffer.setSize(2, 1, false, false, true);
    for (int toggle = 0; toggle < 100; ++toggle) {
        set(p, "bypass", static_cast<float>((toggle + 1) % 2)); constantBlock();
        const float current = buffer.getSample(0, 0);
        require(std::abs(current - previous) < 0.005f, "Master bypass makes an abrupt gain step");
        previous = current;
    }
    set(p, "bypass", 1); buffer.setSize(2, 127, false, false, true);
    for (int block = 0; block < 50; ++block) constantBlock();
    require(std::abs(buffer.getSample(0, 126) - 0.1f) < 0.00005f, "Master bypass does not converge to dry unity");
    require(p.getLatencySamples() == latency, "Bypass transition changes host latency");
}

void silenceAndInvalidInput() {
    GummyAudioProcessor p; p.loadFactoryPreset(9); prepare(p, 2, 48000);
    settle(p, 2, 48000);
    juce::AudioBuffer<float> b(2, 257); b.clear(); process(p, b);
    require(b.getMagnitude(0, b.getNumSamples()) < 1.0e-6f, "Silence creates audible output");
    b.clear();
    b.setSample(0, 0, std::numeric_limits<float>::quiet_NaN());
    b.setSample(0, 1, std::numeric_limits<float>::infinity());
    b.setSample(1, 2, -std::numeric_limits<float>::infinity());
    b.setSample(1, 3, 1.0e20f);
    process(p, b);
    for (int i = 0; i < 20; ++i) { b.clear(); process(p, b); }
    require(std::isfinite(p.inputMeter.load()) && std::isfinite(p.outputMeter.load())
            && std::isfinite(p.reductionMeter.load()) && std::isfinite(p.resonanceMeter.load())
            && std::isfinite(p.clipMeter.load()), "Non-finite meter output");
}

void ceiling() {
    const float ceilingLinear = std::pow(10.0f, -6.0f / 20.0f);
    for (int quality : { 0, 1, 2 })
        for (int mode : { 0, 1 }) {
            GummyAudioProcessor p; p.loadFactoryPreset(0);
            set(p, "quality", static_cast<float>(quality)); set(p, "ceiling", -6);
            set(p, "clipDrive", 24); set(p, "clipMode", static_cast<float>(mode));
            set(p, "clipOn", 1); set(p, "mix", 100); set(p, "output", 0);
            prepare(p, 2, 96000, 64);
            juce::AudioBuffer<float> b(2, 509);
            float peak = 0;
            for (int block = 0; block < 60; ++block) {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < b.getNumSamples(); ++i)
                        b.setSample(c, i, 20.0f * signal(c, block * b.getNumSamples() + i, 96000));
                process(p, b);
                peak = std::max(peak, b.getMagnitude(0, b.getNumSamples()));
            }
            require(peak <= ceilingLinear + 0.000002f, "Final sample peak exceeds selected ceiling");
            require(peak > 0.1f, "Clipper unexpectedly mutes driven signal");
        }
}

void nonlinearAliasMeasurement() {
    // A coherent 10 kHz sine at 48 kHz creates a third harmonic at 30 kHz,
    // which folds to 18 kHz without antialiasing. Measure that exact spectral
    // component after the production nonlinear stage, including its final
    // sample ceiling. This is one defined stress signal, not a true-peak or
    // comprehensive perceptual quality claim.
    constexpr int window = 9600;
    std::array<double, 3> alias {}, fundamental {};
    for (int q = 0; q < 3; ++q) {
        gummy::Settings settings;
        settings.quality = q; settings.clipDrive = 18; settings.ceiling = 0;
        settings.clipMode = 0; settings.clipOn = true;
        gummy::NonlinearStage stage; stage.prepare(48000, 128, true);
        juce::AudioBuffer<float> buffer(2, 128);
        double aliasReal = 0, aliasImaginary = 0, fundamentalReal = 0, fundamentalImaginary = 0;
        for (int position = 0; position < window * 2;) {
            const int n = std::min(128, window * 2 - position);
            for (int i = 0; i < n; ++i) {
                const float x = static_cast<float>(0.8 * std::sin(2.0 * pi * 10000.0 * (position + i) / 48000.0));
                buffer.setSample(0, i, x); buffer.setSample(1, i, x);
            }
            stage.process(buffer, n, settings);
            for (int i = 0; i < n; ++i) {
                const int sample = position + i;
                if (sample < window) continue;
                const double x = buffer.getSample(0, i);
                const double a = 2.0 * pi * 18000.0 * sample / 48000.0;
                const double f = 2.0 * pi * 10000.0 * sample / 48000.0;
                aliasReal += x * std::cos(a); aliasImaginary += x * std::sin(a);
                fundamentalReal += x * std::cos(f); fundamentalImaginary += x * std::sin(f);
            }
            position += n;
        }
        alias[static_cast<std::size_t>(q)] = 2.0 * std::hypot(aliasReal, aliasImaginary) / window;
        fundamental[static_cast<std::size_t>(q)] = 2.0 * std::hypot(fundamentalReal, fundamentalImaginary) / window;
    }
    std::cout << "INFO hard clip alias at 18 kHz, 10 kHz sine / 48 kHz / +18 dB drive: ";
    for (int q = 0; q < 3; ++q)
        std::cout << (q == 0 ? "Off " : q == 1 ? ", 2x " : ", 4x ")
                  << 20.0 * std::log10(std::max(alias[static_cast<std::size_t>(q)], 1.0e-12)
                     / std::max(fundamental[static_cast<std::size_t>(q)], 1.0e-12)) << " dBc";
    std::cout << '\n';
    require(alias[0] > 0.001, "Test signal did not create measurable alias energy");
    require(alias[2] < alias[0], "4x nonlinear stage did not reduce measured 18 kHz alias component");
}

std::vector<float> values(GummyAudioProcessor& p) {
    std::vector<float> result;
    for (auto* parameter : p.getParameters()) result.push_back(parameter->getValue());
    return result;
}
void expectValues(GummyAudioProcessor& p, const std::vector<float>& expected, const char* message) {
    const auto observed = values(p);
    require(observed.size() == expected.size(), message);
    for (std::size_t i = 0; i < observed.size(); ++i)
        require(std::abs(observed[i] - expected[i]) < 0.00001f, message);
}

void stateAndPresets() {
    GummyAudioProcessor p;
    require(p.presetNames().size() == 10, "Factory preset count is not ten");
    std::vector<std::vector<float>> snapshots;
    for (int preset = 0; preset < p.presetNames().size(); ++preset) {
        p.loadFactoryPreset(preset);
        require(p.currentPreset() == preset, "Factory preset index not recalled");
        snapshots.push_back(values(p));
        for (int previous = 0; previous < preset; ++previous)
            require(snapshots.back() != snapshots[static_cast<std::size_t>(previous)], "Duplicate factory parameter snapshot");
        prepare(p, 2, 48000);
        juce::AudioBuffer<float> b(2, 257);
        for (int block = 0; block < 5; ++block) {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 257; ++i) b.setSample(c, i, signal(c, block * 257 + i, 48000));
            process(p, b);
        }
        p.releaseResources();
    }
    p.loadFactoryPreset(3); set(p, "input", 3.25f); set(p, "quality", 2);
    require(p.presetDisplayName().containsChar('*'), "Modified factory preset is not marked");
    const auto savedValues = values(p);
    juce::MemoryBlock state; p.getStateInformation(state);
    require(state.getSize() > 100, "Session state is empty");
    p.loadFactoryPreset(0); p.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    expectValues(p, savedValues, "DAW state did not restore all parameters");
    GummyAudioProcessor restored; restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    expectValues(restored, savedValues, "Fresh instance did not restore state");
    const std::array<char, 5> malformed { 'b', 'r', 'o', 'k', 'e' };
    restored.setStateInformation(malformed.data(), static_cast<int>(malformed.size()));
    expectValues(restored, savedValues, "Malformed state changed parameters");

    const auto presetFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                .getNonexistentChildFile("GUMMY-test-preset", ".gummy", false);
    struct Cleanup { juce::File file; ~Cleanup() { file.deleteFile(); } } cleanup { presetFile };
    require(p.saveUserPreset(presetFile), "Cannot save user preset");
    require(presetFile.getSize() > 100, "Saved user preset is empty");
    require(p.presetDisplayName() == presetFile.getFileNameWithoutExtension(), "Saved user preset name is not displayed without a modified marker");
    p.loadFactoryPreset(0); require(p.loadUserPreset(presetFile), "Cannot load user preset");
    expectValues(p, savedValues, "User preset did not restore all parameters");
    require(p.presetDisplayName() == presetFile.getFileNameWithoutExtension(), "Loaded user preset name is not displayed without a modified marker");
    juce::MemoryBlock namedUserState; p.getStateInformation(namedUserState);
    GummyAudioProcessor namedUserRestore;
    namedUserRestore.setStateInformation(namedUserState.getData(), static_cast<int>(namedUserState.getSize()));
    expectValues(namedUserRestore, savedValues, "DAW state did not preserve user preset parameters");
    require(namedUserRestore.presetDisplayName() == presetFile.getFileNameWithoutExtension(), "DAW state did not preserve the clean user preset name");
    set(namedUserRestore, "input", -3);
    require(namedUserRestore.presetDisplayName() == presetFile.getFileNameWithoutExtension() + " *", "Modified user preset name is not marked");
    require(!p.loadUserPreset(presetFile.getSiblingFile("GUMMY-nonexistent-preset-9817")), "Missing user preset was accepted");
}

void comparisonAndIsolation() {
    GummyAudioProcessor a, b;
    a.loadFactoryPreset(0); b.loadFactoryPreset(0);
    require(!a.isSlotB(), "Initial comparison slot should be A");
    set(a, "input", 3); set(a, "body", 37);
    const auto slotA = values(a);
    a.switchAB(); require(a.isSlotB(), "A/B does not select B");
    set(a, "input", -7); set(a, "body", -22);
    const auto slotB = values(a);
    a.switchAB(); require(!a.isSlotB(), "A/B does not select A");
    expectValues(a, slotA, "A comparison snapshot not restored");
    a.switchAB(); expectValues(a, slotB, "B comparison snapshot not restored");
    require(std::abs(actual(b, "input")) < 0.001f && std::abs(actual(b, "body")) < 0.001f,
            "Parameters are shared between instances");
    a.loadFactoryPreset(9); b.loadFactoryPreset(0); set(b, "quality", 0);
    prepare(a, 2, 48000); prepare(b, 2, 48000);
    juce::AudioBuffer<float> x(2, 257), y(2, 257);
    float difference = 0;
    for (int block = 0; block < 20; ++block) {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 257; ++i) x.setSample(c, i, signal(c, block * 257 + i, 48000));
        y.makeCopyOf(x, true); process(a, x); process(b, y);
        for (int i = 0; i < 257; ++i) difference += std::abs(x.getSample(0, i) - y.getSample(0, i));
    }
    require(difference > 1.0f, "Separate creative and neutral instances produce identical output");
}

void performance() {
    GummyAudioProcessor p; p.loadFactoryPreset(4); set(p, "smooth", 50); set(p, "quality", 2);
    prepare(p, 2, 48000, 128);
    juce::AudioBuffer<float> b(2, 128); int frames = 0;
    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < 750; ++block) {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 128; ++i) b.setSample(c, i, signal(c, frames + i, 48000));
        process(p, b); frames += 128;
    }
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << "INFO stereo 48 kHz / 128 / 4x: " << 100.0 * elapsed / (frames / 48000.0)
              << "% of one core (includes signal generation and output checks; environment-dependent)\n";
}

juce::Component* findControl(juce::Component& component, const juce::String& id) {
    if (component.getComponentID() == id) return &component;
    for (auto* child : component.getChildren())
        if (auto* result = findControl(*child, id)) return result;
    return nullptr;
}

void exerciseControls(GummyAudioProcessor& p, juce::AudioProcessorEditor& editor) {
    for (const char* id : { "input", "output", "mix", "attack", "sustain", "threshold", "ratio",
            "compAttack", "release", "makeup", "compMix", "colorDrive", "colorMix", "body",
            "presence", "air", "smooth", "motionAmount", "clipDrive", "ceiling" }) {
        auto* slider = dynamic_cast<juce::Slider*>(findControl(editor, id));
        require(slider != nullptr, std::string("Missing editor slider ") + id);
        auto* parameter = p.apvts.getParameter(id);
        const float desired = parameter->convertFrom0to1(0.63f);
        slider->setValue(desired, juce::sendNotificationSync);
        require(std::abs(actual(p, id) - desired) < 0.002f, std::string("Editor slider does not update ") + id);
        const float automated = parameter->convertFrom0to1(0.23f);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(automated));
        require(std::abs(slider->getValue() - automated) < 0.002, std::string("Editor slider does not follow automation: ") + id);
    }
    for (const char* id : { "bypass", "transientOn", "compOn", "colorOn", "toneOn", "smoothOn",
                           "motionOn", "clipOn", "autoRelease" }) {
        auto* button = dynamic_cast<juce::Button*>(findControl(editor, id));
        require(button != nullptr, std::string("Missing editor button ") + id);
        const bool desired = !button->getToggleState();
        button->setToggleState(desired, juce::sendNotificationSync);
        require((actual(p, id) > 0.5f) == desired, std::string("Editor button does not update ") + id);
    }
    for (const char* id : { "compType", "colorType", "motionType", "clipMode" }) {
        const int last = std::string(id) == "clipMode" ? 1 : 2;
        auto* button = dynamic_cast<juce::Button*>(findControl(editor, juce::String(id) + "_" + juce::String(last)));
        require(button != nullptr && static_cast<bool>(button->onClick), std::string("Missing editor mode button ") + id);
        button->onClick();
        require(std::abs(actual(p, id) - static_cast<float>(last)) < 0.001f, std::string("Editor mode button does not update ") + id);
    }
    auto* quality = dynamic_cast<juce::ComboBox*>(findControl(editor, "quality"));
    require(quality != nullptr, "Missing editor quality selector");
    quality->setSelectedId(3, juce::sendNotificationSync);
    require(actual(p, "quality") == 2, "Editor quality selector does not update parameter");
}

void editorTest(const juce::File& screenshot) {
    GummyAudioProcessor p; prepare(p, 2, 48000);
    std::atomic<bool> running { true };
    std::exception_ptr audioFailure;
    std::thread audio([&] {
        try {
            juce::AudioBuffer<float> buffer(2, 127);
            while (running.load(std::memory_order_relaxed)) {
                buffer.clear(); process(p, buffer);
            }
        } catch (...) { audioFailure = std::current_exception(); }
    });
    struct StopAudio {
        std::atomic<bool>& running;
        std::thread& thread;
        ~StopAudio() { running.store(false, std::memory_order_relaxed); if (thread.joinable()) thread.join(); }
    } stopAudio { running, audio };
    for (int cycle = 0; cycle < 3; ++cycle) {
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        require(editor != nullptr, "Custom editor was not created");
        for (const auto size : { std::pair<int, int>{900, 600}, {1100, 720}, {1650, 1080} }) {
            editor->setSize(size.first, size.second);
            const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
            require(snapshot.isValid() && snapshot.getWidth() > 0, "Editor snapshot failed after resize");
            if (cycle == 0 && size.first == 1100 && screenshot != juce::File()) {
                require(screenshot.getParentDirectory().createDirectory().wasOk(), "Cannot create screenshot directory");
                auto stream = screenshot.createOutputStream();
                require(stream != nullptr, "Cannot create screenshot file");
                require(stream->setPosition(0) && stream->truncate().wasOk(), "Cannot overwrite screenshot file");
                juce::PNGImageFormat png;
                require(png.writeImageToStream(snapshot, *stream), "Cannot encode editor screenshot");
            }
        }
        exerciseControls(p, *editor);
        p.editorBeingDeleted(editor.get());
    }
    running.store(false, std::memory_order_relaxed); audio.join();
    if (audioFailure) std::rethrow_exception(audioFailure);
}
}

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI initialiseJuce;
    bool gui = false;
    juce::File screenshot;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--gui") gui = true;
        else if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            screenshot = juce::File::getCurrentWorkingDirectory().getChildFile(argv[++i]); gui = true;
        }
    }
    std::cout << "GUMMY processor integration tests\n";
    test("allocation instrumentation detects an allocation", [] {
        using Allocate = void* (*)(std::size_t);
        Allocate volatile allocate = &::operator new;
        const auto before = rt_allocations::count;
        rt_allocations::enabled = true;
        void* probe = allocate(37);
        rt_allocations::enabled = false;
        ::operator delete(probe);
        require(rt_allocations::count == before + 1, "Allocation instrumentation is inactive");
        rt_allocations::count = 0;
    });
    test("neutral unity, mono/stereo, six sample rates, irregular and oversized blocks", neutralAndLatency);
    test("aligned dry, master bypass, host bypass, fixed latency at all quality modes", dryAndBypass);
    test("rapid automation, quality changes, bypass toggles, sample-rate reprepare; no callback allocations", automateAndReprepare);
    test("click-free master bypass transitions converge to aligned dry unity", bypassTransition);
    test("silence, extreme/non-finite input and finite meters", silenceAndInvalidInput);
    test("hard/soft sample-peak ceiling at Off/2x/4x", ceiling);
    test("production clipping-stage alias measurement at Off/2x/4x", nonlinearAliasMeasurement);
    test("ten distinct factory presets, modified names, DAW state and user preset round-trip", stateAndPresets);
    test("A/B snapshots and multiple-instance isolation", comparisonAndIsolation);
    test("full-chain CPU observation", performance);
    if (gui) {
       #if JUCE_LINUX || JUCE_BSD
        if (std::getenv("DISPLAY") == nullptr) std::cout << "SKIP GUI: DISPLAY is unset\n";
        else
       #endif
            test("editor lifecycle, resizing, rendering, all control attachments, and concurrent audio", [&] { editorTest(screenshot); });
    }
    std::cout << passed << " passed, " << failed << " failed; " << callbacks
              << " process callbacks checked, " << rt_allocations::count << " counted C++ audio-thread allocations\n";
    std::cout << "NOTE allocation instrumentation covers C++ new/new[] on the calling thread; it does not intercept direct malloc or prove absence of locks/file I/O.\n";
    return failed == 0 ? 0 : 1;
}
