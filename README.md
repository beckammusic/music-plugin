# GUMMY — SWEETEN YOUR SOUND

Native C++17/JUCE 8.0.6 audio effect. Seven processors run in this order:

Input → Transient → Compress → Color → Tone → Smooth → Motion → Clip → Output.

This repository implements DSP, a custom native editor, AU/VST3/Standalone CMake targets, automation, state, presets, tests, and macOS packaging scripts. The development environment is Linux. The macOS AU target and Logic Pro compatibility must be built and verified on a Mac before release. See `Documentation/DEVELOPMENT_REPORT.md` for actual results and pending checks.

## Get a Mac build without installing developer tools

Open the repository's **Actions → Build macOS plugin → Run workflow**. A successful run provides the **GUMMY-macOS** download with an AU, VST3, Standalone app, and double-click installer. The workflow builds both Apple Silicon and Intel slices, runs CTest and AU validation, and uploads logs. It uses ad-hoc development signing; Developer ID signing/notarization and actual Logic testing are separate release gates.

Follow [the GitHub build and installation guide](Documentation/GITHUB_BUILD.md). This workflow has been prepared and locally checked; macOS build success must be established by an actual GitHub Actions run.

## Build on macOS

Install Xcode with its command-line tools, CMake 3.22+, and Git. Accept Xcode's license and select the active Xcode installation. From this directory:

```sh
./Scripts/build_mac.sh --arch native
# Optional universal release:
./Scripts/build_mac.sh --arch universal
```

The script fetches pinned JUCE 8.0.6 if a checkout is not supplied, builds, runs CTest, and signs local bundles ad hoc by default. Run `./Scripts/build_mac.sh --help` for architecture, build-directory, JUCE-checkout, and signing options. A network connection is required for the first dependency download. Universal builds require an SDK supporting arm64 and x86_64.

Expected outputs:

- `build-mac/GUMMY_artefacts/Release/AU/GUMMY.component`
- `build-mac/GUMMY_artefacts/Release/VST3/GUMMY.vst3`
- `build-mac/GUMMY_artefacts/Release/Standalone/GUMMY.app`

Install the built bundles and validate their actual installed metadata:

```sh
./Scripts/install_mac.sh
./Scripts/validate_au.sh
```

Restart Logic, open Plug-in Manager, and select **Audio FX → Audio Units → GUMMY Audio → GUMMY**. Full beginner instructions and troubleshooting are in `Documentation/LOGIC_INSTALL.md`. Uninstall with `./Scripts/uninstall_mac.sh`; confirmation is required and user presets are preserved.

Distribution requires choosing a source license and complying with JUCE and bundled VST3 SDK terms. Public macOS distribution also requires appropriate Developer ID signing/notarization. See `Documentation/LICENSING.md`; no license entitlement or notarization is supplied by this source package.

## CMake directly

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 3
ctest --test-dir build -C Release --output-on-failure
```

To use a local checkout, add `-DGUMMY_JUCE_PATH=/absolute/path/to/JUCE`. AU is enabled on Apple platforms; Linux builds VST3 and Standalone. Float processing is supported. Mono input maps to mono output, stereo to stereo; there is no MIDI processing or sidechain bus.

On Debian/Ubuntu, native builds need a compiler, CMake, pkg-config, ALSA, X11/Xext/Xrandr/Xinerama/Xcursor, FreeType, Fontconfig, and OpenGL development packages. Browser and curl integrations are disabled. JUCE is fetched at commit `51a8a6d7aeae7326956d747737ccf1575e61e209`.

The DSP suite can build without JUCE or GUI dependencies:

```sh
cmake -S . -B build-dsp -DGUMMY_BUILD_PLUGIN=OFF
cmake --build build-dsp
ctest --test-dir build-dsp --output-on-failure
```

## Controls

Each module has an illuminated **ON** switch with text as well as color feedback. Drag knobs normally; use Shift for fine control, double-click to enter a value, or right-click to reset. Keyboard arrow controls are supported. Hover for descriptions. The editor scales from 880×576 to 1650×1080.

- **Transient** separates attack impact from sustain/decay using linked fast/slow envelope detection.
- **Compress** offers Smooth RMS leveling, Punch peak dynamics, and Glue bus compression; its wet/dry mix runs before the next module.
- **Color** uses distinct Tape, Tube, and Color nonlinear curves with deterministic drive compensation and DC blocking.
- **Tone** has broad bipolar Body, Presence, and Air curves; zero is neutral.
- **Smooth** uses a selective adaptive filter bank with neighboring-band comparison and a tonal-content guard. It is a conservative zero-latency suppressor, with finite frequency resolution rather than arbitrary spectral surgery.
- **Motion** blends Chorus, Flanger, or Doubler voices. Zero amount is dry; type changes fade between continuously running states.
- **Clip** offers hard and soft curves with a sample-peak ceiling. It is not a true-peak limiter. Output gain or global dry blending can exceed the internal clip ceiling. Ceiling automation is smoothed; the new ceiling settles over the control smoothing interval.

Global Mix and Master Bypass blend against the original input, before Input/Output gain. Both paths retain fixed host latency. Off, 2x, and 4x quality choices apply only to Color and Clip. FIR stages and padding maintain 120 samples of latency at every quality setting. All three quality banks remain prepared and running so automation requires no audio-thread reconstruction; this trades CPU for stable transitions. Off disables upsampling but retains alignment delay. FIR reconstruction filters introduce their intended high-frequency magnitude rolloff near Nyquist.

## Presets and sessions

The header browses ten actual factory parameter snapshots, saves/loads `.gummy` XML user presets, and switches A/B snapshots. An asterisk marks edits. Factory presets are starting points and are not loudness-normalized across arbitrary material. Ten loadable snapshots also live in `Assets/FactoryPresets`.

DAW state uses a versioned APVTS tree and stores comparison slots. State files are validated before recall. Presets and instances have independent DSP state. The GUI never writes DSP internals directly.

Stable identity: manufacturer **GmAu**, plugin code **Gmmy**, bundle ID **com.gummyaudio.gummy**, AU type **aufx**, version **1.0.0**. Do not change identity or IDs from `Documentation/PARAMETERS.md` in compatible updates.

## Verification

`GummyDSPTests` measures module behavior with synthetic audio. `GummyPluginTests` tests the real AudioProcessor, latency, allocation gates, automation, session/preset recall, sample ceilings, alias reduction, and multiple rates/block sizes. `GummyFormatTests` loads the compiled VST3 through JUCE's host interface. Python tests exercise macOS script contracts through mocked commands. Actual Apple binaries, `auval`, and Logic playback remain platform-specific release gates.

To render and exercise the native editor while a simulated audio thread processes:

```sh
build/GummyPluginTests_artefacts/Release/GummyPluginTests --gui --screenshot /absolute/path/GUMMY.png
```

On headless Linux, provide an X server/Xvfb via `DISPLAY`. These tests do not require an audio interface. CPU observations in the report are development-machine measurements, not a performance guarantee.

Algorithm notes: `Documentation/DYNAMICS.md`, `Documentation/ADVANCED_DSP.md`, `Documentation/ARCHITECTURE.md`.
