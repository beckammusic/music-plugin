# GUMMY in Logic Pro

GUMMY is a native audio effect. Logic Pro uses the **Audio Unit** bundle, `GUMMY.component`; it does not load the VST3 version. Building and running an AU requires a Mac. The Linux build and mocked script tests do not establish macOS or Logic Pro compatibility. Actual macOS compilation, Apple validation, and a Logic Pro session test remain pending until performed on a Mac.

## 1. Build or obtain the plugin

For a downloaded build, obtain a signed build from a source you trust and follow its installation instructions. Preserve the complete `GUMMY.component` bundle; do not copy just its internal executable.

To build from source, install Xcode, launch it once, and accept its license. Select the Xcode developer directory if needed with Xcode's Settings → Locations → Command Line Tools. Install CMake 3.22 or newer, including its `ctest` command, then run:

```sh
cd /path/to/GUMMY
./Scripts/build_mac.sh --jobs 4
```

CMake fetches the pinned JUCE 8.0.6 revision if no local checkout is found; the initial fetch requires Git and network access. For an existing JUCE 8.0.6 checkout, pass `--juce /path/to/JUCE`. The script checks a supplied local checkout's declared version before configuring.

The script uses Xcode's macOS SDK and builds Release AU, VST3 and Standalone targets plus the DSP, processor-integration and VST3-host test executables. It runs the registered CTest suite before signing, then checks each product's code signature after signing. Its default is ad-hoc signing for development on your own Mac; no paid Apple Developer account is needed. The script reports failure if configuration, compilation, tests, signing or signature verification fails. It disables automatic Xcode signing and signs the completed products explicitly. The script contract tests are also included in CTest when CMake finds Python 3; those command mocks do not perform Apple validation.

The default architecture is your Mac's native architecture. `--arch arm64`, `--arch x86_64` and `--arch universal` select Apple Silicon, Intel and both architectures respectively. Your Xcode installation and all dependencies must support the requested architectures. Use a separate `--build-dir` for each architecture or toolchain configuration.

Expected default products:

```text
build-mac/GUMMY_artefacts/Release/AU/GUMMY.component
build-mac/GUMMY_artefacts/Release/VST3/GUMMY.vst3
build-mac/GUMMY_artefacts/Release/Standalone/GUMMY.app
```

These paths describe build outputs, not prebuilt binaries promised in a source-only delivery. Check the development report for the artifacts actually produced in your environment.

## 2. Install into your user Library

Quit Logic Pro and other audio hosts, then run from the GUMMY source directory:

```sh
./Scripts/install_mac.sh
```

For another build directory, use `./Scripts/install_mac.sh --build-dir /path/to/build-mac`. The script requires both compiled Release formats, verifies their identities and signatures, and uses `ditto` to copy the complete bundles into:

```text
~/Library/Audio/Plug-Ins/Components/GUMMY.component
~/Library/Audio/Plug-Ins/VST3/GUMMY.vst3
```

Run it as your regular macOS user, without `sudo`. It checks both source bundles and any existing GUMMY targets before replacing either installed format. It refuses unrelated bundle identifiers or symbolic-link targets. Each bundle is staged and verified before replacement. A replaced GUMMY version is preserved in a uniquely named folder under the corresponding plugin directory's `.GUMMY-backups` folder. The script prints that backup's exact path. It reports any copy failure; a failure on the second format can leave the first format successfully updated, so read the complete output before retrying.

For manual AU-only installation, use Finder's Go → Go to Folder, enter `~/Library/Audio/Plug-Ins/Components/`, create the directory if necessary, and copy the entire `GUMMY.component` bundle there. Check the identity of an existing same-name bundle before replacing it. Do not install a second copy in `/Library/Audio/Plug-Ins/Components/`; duplicate identifiers can make the host select a different build.

## 3. Restart Logic Pro and validate

Restart Logic Pro so that it scans the new AU. From the source directory, run:

```sh
./Scripts/validate_au.sh
```

This script reads the actual `AudioComponents` entry from the installed bundle's `Contents/Info.plist`. It confirms the expected GUMMY identity, checks that `auval -a` lists those identifiers, and invokes `auval -v` using the values it read. It propagates validation failure. If `pluginval` is in your `PATH`, it also runs that tool at strictness level 5. To select an installed pluginval executable explicitly:

```sh
./Scripts/validate_au.sh --pluginval /Applications/pluginval.app/Contents/MacOS/pluginval
```

Open **Logic Pro → Settings → Plug-in Manager** (called Preferences in some older Logic versions), find **GUMMY** under **GUMMY Audio**, and confirm successful validation. If it was scanned before you replaced the bundle, select it and use **Reset & Rescan Selection**. A passing command-line validation is useful evidence, but does not replace testing in Logic Pro itself.

## 4. Insert GUMMY and begin processing

1. Open a Logic project and select an audio track, software instrument channel strip, bus, or stereo output.
2. In the channel strip, click an empty **Audio FX** insert slot.
3. Choose **Audio Units → GUMMY Audio → GUMMY**.
4. Select the matching **Mono** or **Stereo** configuration. Supported layouts are mono-to-mono and stereo-to-stereo.
5. Open the editor, start playback, choose a factory preset, and adjust the controls. Begin with moderate listening levels.

For a first compatibility check, use an audio track with a known recording. Confirm the input and output meters move, enable a module, and make an audible adjustment. The clean initial preset is deliberately neutral; zero module amounts, module bypass, master bypass, or a 0% global Mix can all produce unchanged audio.

## 5. Check project recall and automation

Save a project containing a GUMMY instance with several changed values. Close the project, reopen it, and confirm the values and sound recall. Add an automation lane for a GUMMY parameter and check that playback drives both the control and audible processing. Test a mono track, a stereo track, and multiple independent instances. Open and close the editor during playback. Repeat at the sample rates and buffer sizes you use. A host validation pass alone does not verify these behaviors.

Do not change the following identifiers between releases; existing sessions and automation depend on them:

| Identity | Value |
| --- | --- |
| Product | GUMMY |
| Manufacturer display name | GUMMY Audio |
| Version | 1.0.0 |
| Bundle identifier (AU and VST3) | `com.gummyaudio.gummy` |
| AU type | `aufx` |
| AU manufacturer | `GmAu` |
| AU subtype / plugin code | `Gmmy` |

APVTS parameter IDs are defined in the source parameter table. Keep those IDs stable too. The validation script uses the bundle's metadata, rather than substituting this table into the `auval` command.

## Troubleshooting

**GUMMY does not appear.** Confirm `~/Library/Audio/Plug-Ins/Components/GUMMY.component` exists as a complete bundle, and that Logic was restarted after installation. Check Plug-in Manager and use Reset & Rescan Selection. Run `./Scripts/validate_au.sh` and read the first failure. Check for an older duplicate AU in the system-wide Components directory. If registration remains stale, log out and back in or restart the Mac. Do not delete the entire Audio Unit cache or unrelated plugins as a first step.

**AU validation fails.** Preserve the command output, macOS version, Logic version, build architecture, sample rate and buffer size. Verify the installed binary is the one just built. Rebuild from the pinned JUCE version and rescan. Do not use a failed build on an important session; restore the previous GUMMY bundle from the printed `.GUMMY-backups` path while audio hosts are closed, then rescan.

**Architecture mismatch.** Inspect the binary:

```sh
file "$HOME/Library/Audio/Plug-Ins/Components/GUMMY.component/Contents/MacOS/GUMMY"
lipo -archs "$HOME/Library/Audio/Plug-Ins/Components/GUMMY.component/Contents/MacOS/GUMMY"
```

Use an arm64 or universal build for native Apple Silicon operation and x86_64 or universal for Intel operation. When investigating a host running under Rosetta, verify the host's architecture as well. Prefer a matching native build.

**Missing dependencies.** Run `otool -L` on the same internal binary. System frameworks should resolve on the target macOS version. An external non-system library must be included and signed correctly by the distributor; installing a library at the developer's absolute build path is not a distributable fix. Rebuild with the documented dependencies and deployment target.

**Signature or macOS security error.** Check the original bundle with `codesign --verify --strict --verbose=2 /path/to/GUMMY.component`. If it fails, rebuild and sign it or obtain an intact signed build. Modifying a signed bundle invalidates its signature. Ad-hoc signing is intended for local development; it is not Developer ID notarization for public downloads. For distributed builds, ask the distributor for a signed and notarized release. Follow macOS's normal trusted-software approval flow where applicable; disabling Gatekeeper or recursively removing quarantine attributes is not the default remedy.

**The GUI fails to open.** Close and reopen the editor, then try a new empty Logic project with one instance. Confirm the same problem occurs with the Standalone build, if available. Save Logic crash reports from Console or `~/Library/Logs/DiagnosticReports/`. A host may offer its generic Controls view while investigating, but the custom editor still requires a successful real-host test.

**Audio passes through without processing.** Confirm the track has audio, the insert is enabled, master bypass is off, the desired module is enabled, and global Mix is above 0%. The initial preset and zero amount controls are neutral. Check the module's own Mix/Drive/Amount controls and input/output meters. Check that Logic is loading the intended installed build rather than a duplicate.

**Session settings do not recall.** Save and reopen a minimal project, confirm the same plugin version and stable bundle/parameter identifiers are loaded, and inspect whether host automation is overwriting the value. Preserve a copy of the failing project before changing the plugin build. Report the parameter values and exact steps; do not rename IDs to work around a recall failure.

## Uninstall

Quit Logic Pro and other hosts, then run:

```sh
./Scripts/uninstall_mac.sh
```

It lists only identified GUMMY AU/VST3 bundles in your user Library and requires you to type `REMOVE`. It preserves user presets and `.GUMMY-backups`. An unrelated same-name bundle is refused. Restart Logic Pro afterwards. Existing projects that use GUMMY will report a missing plugin until it is reinstalled.

## Signing and packaging for distribution

Before public distribution, resolve the license choices described in [LICENSING.md](LICENSING.md), complete the real Mac/Logic tests above, and test a fresh user installation. Keep signing credentials in your own Keychain or CI secret store. Never put certificates, private keys, passwords, Apple account tokens, or signing profiles containing secrets into the repository.

For a Developer ID build, use an existing identity in your Keychain:

```sh
./Scripts/build_mac.sh --juce /path/to/JUCE --arch universal \
  --sign "Developer ID Application: Your Legal Name (TEAMID)"
```

The script signs with the hardened runtime and a secure timestamp. If you later add embedded libraries or helper executables, sign each nested code object first and the outer bundle last. Do not rely on `codesign --deep` to repair a distribution package. Verify the final product signatures after all bundle modifications.

Stage the signed bundles and installation instructions in a release directory. A notarized disk image is a practical container for plugins because a ZIP cannot be stapled. The following commands assume `release/GUMMY-1.0.0/` already contains the final signed products and instructions, and that the `GUMMY-NOTARY` Keychain profile has been configured locally using Apple's documented notarytool authentication procedure:

```sh
hdiutil create -volname GUMMY-1.0.0 -srcfolder release/GUMMY-1.0.0 \
  -ov -format UDZO release/GUMMY-1.0.0.dmg
codesign --force --sign "Developer ID Application: Your Legal Name (TEAMID)" \
  --timestamp release/GUMMY-1.0.0.dmg
xcrun notarytool submit release/GUMMY-1.0.0.dmg --keychain-profile GUMMY-NOTARY --wait
xcrun stapler staple release/GUMMY-1.0.0.dmg
xcrun stapler validate release/GUMMY-1.0.0.dmg
```

Inspect the notarization log if the result is not Accepted. A notarized disk image does not establish plugin audio correctness. Test the downloaded container and copied plugins on a clean supported Mac, including the first launch and Logic scan. No Developer ID signature, notarization submission, or Logic Pro result is implied by these instructions.
