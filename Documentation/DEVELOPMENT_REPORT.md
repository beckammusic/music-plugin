# GUMMY 1.0.0 development report

GUMMY is an implemented native C++17/JUCE8 audio effect with a working Linux VST3 and Standalone build. All available automated suites pass. This is a validated Linux development candidate, not a verified macOS release: an AU has not been compiled, installed, validated with auval, or loaded into Logic Pro in this Linux environment. The specification's full definition of done therefore remains open.

## Delivered files and exact locations

The final project is copied to `/workspace/music-plugin` with complete original C++ source, CMake project, DSP, GUI, factory presets, tests, documentation, and scripts.

| Deliverable | Location |
|---|---|
| Source archive | `/workspace/shared/downloads/GUMMY-1.0.0-source.zip` |
| Linux binary package | `/workspace/shared/downloads/GUMMY-1.0.0-linux-x86_64.zip` |
| Linux VST3 bundle | `/workspace/music-plugin/Binaries/Linux-x86_64/GUMMY.vst3` |
| Linux Standalone | `/workspace/music-plugin/Binaries/Linux-x86_64/GUMMY` |
| Native editor screenshot | `/workspace/music-plugin/Assets/GUMMY-editor.png` |
| Build tree / test executables | `/workspace/work/build-linux` |
| macOS AU output when built | `<project>/build-mac/GUMMY_artefacts/Release/AU/GUMMY.component` |
| macOS VST3 output when built | `<project>/build-mac/GUMMY_artefacts/Release/VST3/GUMMY.vst3` |

Linux artifacts are ELF x86_64 binaries built against this Debian13 environment. They are not macOS bundles. The VST3 directory must remain intact. No `.component` or macOS `.vst3` is supplied. The configured projectless chat output directory was absent and could not be created beneath the read-only root, so deliverables are retained in the available shared workspace paths above.

## Executed validation

- GCC14.2.0, CMake4.4.4, pinned JUCE8.0.6 commit `51a8a6d7aeae7326956d747737ccf1575e61e209`; Release build succeeded. Source was inspected for unfinished DSP placeholders.
- CTest: **4/4 suites passed**: DSP, MacScriptContracts, PluginIntegration, VST3BinaryIntegration. Final run17.00seconds.
- Standalone DSP tests and ASan+UBSan passed. LeakSanitizer cannot operate under this environment's ptrace configuration and was disabled; this is not a completed leak check.
- Processor/native editor harness: **12/12 groups passed**, **7,424 process callbacks**, **0 counted C++ callback allocations**. It exercises all34 parameter attachments in both UI and host directions, editor creation/deletion with concurrent simulated audio, rendering/resizing, six sample rates44.1–192kHz, mono/stereo, irregular sizes including deliberate blocks larger than preparation hints, bypass, quality changes, rapid automation, extreme/non-finite samples, silence, ceilings, factory/user presets, A/B, DAW state, filename recall, and instance isolation.
- Independent actual VST3 host: discovery, automation/state for35 exposed host parameters (34 GUMMY controls plus JUCE's Program selector),432 processing callbacks across six rates and mono/stereo, and state recall into a second instance passed. The host test declares a2,053-sample maximum and processes variable sizes1–2,053 within that contract. Deliberate blocks beyond preparation hints are covered separately by the direct processor suite.
- The actual VST3 reports **120 samples latency after preparation**, independent of quality selection;2.50ms at48kHz. pluginval's earlier information query prints0 before the hosting wrapper prepares the processor; this is distinct from its prepared processing latency.
- pluginval1.0.4 **strictness level5: SUCCESS**, including processing, automation, state, editor, and bus tests. This was Linux VST3 validation. Its auval-named test is a platform no-op on Linux; no Apple AU validation occurred. The optional Steinberg validator was unavailable and skipped by pluginval.
- macOS scripts: **17 mocked command/filesystem behavior tests passed**, plus shell syntax checks. Actual Apple executables, signing, SDK builds, installation, and validation were not exercised.
- The final native1100×720 screenshot was visually inspected; numeric contrast and all module layouts passed review. Source uses vector drawing and scalable bounds. Actual Retina hardware remains untested.

Clipping measurement: a coherent10kHz sine at48kHz, amplitude0.8, +18dB drive, with settling before a9,600-sample DFT window, yielded an18kHz alias of **Off−9.9559dBc;2x−21.9372dBc;4x−21.0503dBc**. Both oversampled settings reduced this alias;4x improved it by11.09dB over Off. This single measurement does not establish monotonic improvement at every quality setting or all frequencies.

The full-chain stereo48kHz/128sample/4x harness observation was18.65% of one core, including signal generation and output checking. This is environment-dependent elapsed time under development load, not an isolated DSP benchmark or a worst-case scheduling guarantee. A slower host,192kHz processing, or many instances can differ substantially.

The allocation gate intercepts C++new/new[] on the calling thread. It does not intercept arbitrary malloc, file I/O, or locks. Code inspection found none of those operations in processBlock/Engine processing; state-only synchronization and file serialization stay outside the callback.

## Feature table

Status **Tested** means exercised in this Linux environment; **Implemented** means code supplied with platform verification pending; **Pending** means unavailable or not yet verified.

| Feature | Status | Evidence / practical scope |
|---|---|---|
| Native C++17/JUCE8/CMake project | Tested | Native Release VST3/Standalone compilation; Evidence/build-linux*.txt |
| Transient attack/sustain | Tested | DynamicsTests: unity, impulses, transient/decay effects, sample rates |
| Smooth/Punch/Glue compression | Tested | Knees, threshold/ratio, linking, timing, auto release, mix, mode differences |
| Tape/Tube/Color saturation | Tested | Nonlinear curves, DC, mode/drive differences, oversampled processing |
| Body/Presence/Air EQ | Tested | Measured neutral and boost/cut responses at multiple frequencies/rates |
| Selective Smooth resonance bank | Tested | Synthetic resonance/noise selectivity, tonal guard, linking, attenuation/fade stability |
| Chorus/Flanger/Doubler | Tested | Delay/pitch movement, stereo/mono behavior, transitions, bounded feedback/tail decay |
| Hard/Soft clipping | Tested | Transfer curves and steady sample ceilings at all qualities, extreme signals, alias measurement |
| Input/Output/Mix/Master Bypass | Tested | Gain and original-input routing, click-free bypass convergence, aligned dry and host bypass |
| Independent module bypass | Tested | Rapid automation and native UI switch attachments; latency preserved |
| Off/2x/4x oversampling | Tested | FIR banks, fractional delay at high rate, fixed120sample reporting, quality automation |
| Peak/GR/resonance/clip meters | Tested | Finite atomics, selected-path activity; native timer/rendering exercised |
|34 stable automation parameters | Tested | Direct APVTS, full UI attachment exercise, actual VST3 state/automation |
|10 actual factory presets | Tested | Distinct saved settings, browsing/recall, clean initialization |
| User preset save/load and displayed filename | Tested | XML snapshot, filename and modified marker, DAW round-trip |
| A/B and multiple independent instances | Tested | Comparison snapshots, host save/restore, actual second VST3 instance |
| Native Y2K editor, resize/accessibility controls | Tested | Numeric entry/reset/tooltips/keyboard implemented;34 attachments and rendering exercised; PNG reviewed |
| Mono/stereo;44.1–192kHz; variable blocks | Tested | Direct processor and independent VST3 host; no native audio device required |
| Float stability / callback memory discipline | Tested | Sanitized input, denormal guard, allocation gate, DSP sanitizers, inspection |
| Linux VST3 and Standalone packaging | Tested | Real ELF products; actual VST3 host+pluginval; Standalone linked |
| AU and macOS VST3 targets | Implemented | Apple-conditional CMake formats; cannot compile Apple targets with Linux toolchain |
| macOS build/install/uninstall/validation scripts | Implemented |17 mocked tests and shell parsing passed; real macOS execution pending |
| Apple Silicon/Intel/universal builds | Pending | Architecture options supplied; Apple SDK builds must run on a Mac |
| macOS installation/signing/notarization | Pending | User-level scripts/docs supplied; no real signature or notarization claimed |
| auval and Audio Unit discovery | Pending | Metadata-derived validator supplied; Apple tools unavailable |
| Logic Pro playback, inserts, automation, project recall | Pending | Beginner guide supplied; Logic Pro unavailable |
| Professional listening and broad source-material tuning | Pending | Synthetic numerical validation is not a substitute for listening across material |
| Retina hardware / long-duration DAW stress | Pending | Scalable vectors implemented; physical-device/host soak testing remains |
| Original project distribution license | Pending | Owner must select; JUCE/VST3 obligations documented in LICENSING.md |

Optional8x quality, double-precision processing, true-peak limiting, AUv3/AAX/CLAP, and analog circuit emulation are not implemented. Float AU/VST3 audio effects are the defined initial scope.

## Build, install, open, test

On a Mac, from the project root:

```sh
./Scripts/build_mac.sh --arch native
# Or --arch universal with a suitable Xcode SDK.
./Scripts/install_mac.sh
./Scripts/validate_au.sh
```

The build script uses a supplied JUCE8.0.6 checkout or CMake's pinned FetchContent path, builds all products/tests, runs CTest, and signs local bundles. A failed test stops the script. Restart Logic, open Plug-in Manager and confirm validation, then choose **Audio FX → Audio Units → GUMMY Audio → GUMMY**. Test mono/stereo tracks, an instrument insert, a bus, and the stereo output. Exercise automation, transport, project save/reopen, multiple instances, and editor lifecycle before release. See LOGIC_INSTALL.md for complete beginner instructions and troubleshooting.

Local original compiled products:

```text
/workspace/work/build-linux/GUMMY_artefacts/Release/VST3/GUMMY.vst3
/workspace/work/build-linux/GUMMY_artefacts/Release/Standalone/GUMMY
/workspace/work/build-linux/GummyPluginTests_artefacts/Release/GummyPluginTests
/workspace/work/build-linux/GummyFormatTests_artefacts/Release/GummyFormatTests
```

General verification command: `ctest --test-dir <build> -C Release --output-on-failure`. On headless Linux, use Xvfb/DISPLAY for native editor/host operations. All evidence logs are under `Documentation/Evidence`; the native screenshot is `Assets/GUMMY-editor.png`. SHA-256 hashes identify final packaged binaries in Evidence/artifact-sha256.txt.

The source and a stable tested Linux build are preserved. The next release gate is a real Mac build and Apple-host validation, followed by listening and distribution-license/signing decisions.
