# Download GUMMY for Logic using GitHub Actions

Repository: https://github.com/beckammusic/music-plugin

You do not need Xcode, CMake, or a compiler on your own Mac to use this route. GitHub's macOS runner builds the plugin and produces a downloadable development package. GitHub Actions usage is subject to your account's available minutes and settings.

## Run a build

1. Open https://github.com/beckammusic/music-plugin/actions/workflows/build-macos.yml while signed in.
2. Select **Run workflow**, keep the **main** branch selected, and press the green **Run workflow** button.
3. Open the new run and wait for **Universal AU and VST3** to finish. The first build can take several minutes.
4. On the run's summary page, scroll to **Artifacts** and download **GUMMY-macOS**. Stay signed in to download artifacts.
5. Extract that download, then extract the inner **GUMMY-macOS.zip**. Keep the resulting folder intact. The inner ZIP preserves executable permissions and plugin bundle metadata that GitHub's outer artifact format does not preserve directly.

Builds run only when manually requested. A build failure prevents the installable artifact from being uploaded. The separate **GUMMY-macOS-logs** artifact is retained even when a build or test fails. Artifacts expire after 30 days; run the workflow again to create a fresh copy.

## Install and open in Logic

1. Quit Logic Pro and other audio hosts.
2. Open the extracted **GUMMY-macOS** folder and double-click **Install GUMMY.command**. It installs GUMMY in your own Library and does not need administrator privileges.
3. Double-click **Validate GUMMY.command**. Keep the output if validation fails.
4. Restart Logic Pro. Open **Logic Pro → Settings → Plug-in Manager** and confirm **GUMMY** is enabled and passes validation.
5. Open an audio track, instrument channel strip, or bus. Choose **Audio FX → Audio Units → GUMMY Audio → GUMMY** and the appropriate mono/stereo configuration.
6. Start with **INIT / CLEAN START** or a factory preset. Test audio, automation, GUI opening/closing, project save/reopen, and multiple instances on your own Mac.

The same package supports Apple Silicon and Intel Macs, targeting macOS 11 or later. CI checks that all three built binaries contain both architectures. The runner executes tests for its own architecture; actual Logic Pro behavior and playback on your particular Mac still need verification.

The package includes a Standalone app and VST3 for other uses. Logic uses the **Audio Unit**, `GUMMY.component`.

To remove the plugin later, double-click **Uninstall GUMMY.command**, read the prompt, and enter **REMOVE**. User presets are retained.

## Signing and macOS security

The workflow uses ad-hoc development signatures and verifies them. It does not provide an Apple Developer ID signature or notarization. Files downloaded from a browser may be quarantined and macOS may block their first launch. Follow the contextual instructions in **System Settings → Privacy & Security** only for your own trusted build, or obtain a Developer ID-signed/notarized package. Do not disable Gatekeeper globally. See `LOGIC_INSTALL.md` for troubleshooting.

A certificate and notarization credentials are not needed to run the development workflow. A broadly distributed release needs the licensing and signing decisions described in `LICENSING.md`.

## What the workflow checks

- Pinned JUCE 8.0.6 source at commit `51a8a6d7aeae7326956d747737ccf1575e61e209`.
- Universal native AU, VST3, and Standalone compilation.
- Automated DSP, processor, actual VST3 host, and script-contract tests via CTest.
- Native editor lifecycle, all parameter attachments, and a rendered screenshot included with the logs.
- Both arm64 and x86_64 slices via `lipo`.
- User-level installation on the runner, AU discovery and metadata-derived `auval` validation.
- Signature verification and a ZIP containing intact bundles, installer/uninstaller/validator launchers, documentation, license notices, and SHA-256 checksum.

Read-only repository permissions are sufficient for the workflow; it uploads run artifacts and does not publish GitHub releases. The workflow's first actual macOS execution, AU results, and the resulting download can only be confirmed from a completed run. A green build does not imply that Logic Pro itself was tested on the runner.

## If the build fails

Open the failed step and download **GUMMY-macOS-logs**. Share the run URL and the failed step's error output. A failure is actionable evidence; do not install a partially produced bundle. If **Run workflow** is missing, ensure the workflow is on the repository's default branch and Actions is enabled for the repository.
