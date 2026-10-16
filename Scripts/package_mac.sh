#!/bin/bash
# Build a permission-preserving ZIP containing the signed bundles and installer.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
source "$SCRIPT_DIR/mac_common.sh"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
build_dir="$PROJECT_DIR/build-mac"
output_dir="$PROJECT_DIR/dist"
usage() { printf 'Usage: Scripts/package_mac.sh [--build-dir PATH] [--output-dir PATH]\nPackages existing signed macOS AU, VST3 and Standalone bundles.\n'; }
while [ "$#" -gt 0 ]; do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --build-dir|--output-dir)
            [ "$#" -ge 2 ] && [ -n "$2" ] || gummy_fail "Missing value after $1"
            if [ "$1" = --build-dir ]; then build_dir="$2"; else output_dir="$2"; fi
            shift 2 ;;
        *) gummy_fail "Unknown option: $1 (use --help)" ;;
    esac
done
gummy_require_mac
gummy_plist_tool
for required in ditto codesign shasum; do gummy_need "$required"; done
for relative in 'AU/GUMMY.component' 'VST3/GUMMY.vst3' 'Standalone/GUMMY.app'; do
    format="${relative%%/*}"
    bundle="$build_dir/GUMMY_artefacts/Release/$relative"
    gummy_verify_bundle "$bundle" "$format"
    codesign --verify --strict "$bundle" || gummy_fail "Invalid bundle signature: $bundle"
done
staging_root="$(mktemp -d "${TMPDIR:-/tmp}/gummy-package.XXXXXX")"
trap 'rm -rf "$staging_root"' EXIT
package_root="$staging_root/GUMMY-macOS"
mkdir -p "$package_root/build-mac/GUMMY_artefacts/Release" "$package_root/Scripts" "$package_root/Documentation"
for relative in 'AU/GUMMY.component' 'VST3/GUMMY.vst3' 'Standalone/GUMMY.app'; do
    mkdir -p "$package_root/build-mac/GUMMY_artefacts/Release/${relative%%/*}"
    ditto "$build_dir/GUMMY_artefacts/Release/$relative" "$package_root/build-mac/GUMMY_artefacts/Release/$relative"
done
for script in mac_common.sh install_mac.sh uninstall_mac.sh validate_au.sh; do
    cp "$SCRIPT_DIR/$script" "$package_root/Scripts/$script"
    chmod 755 "$package_root/Scripts/$script"
done
for doc in LOGIC_INSTALL.md LICENSING.md; do
    cp "$PROJECT_DIR/Documentation/$doc" "$package_root/Documentation/$doc"
done
ditto "$PROJECT_DIR/Documentation/ThirdParty" "$package_root/Documentation/ThirdParty"
cp "$PROJECT_DIR/LICENSE-NOTES.md" "$package_root/LICENSE-NOTES.md"
cat > "$package_root/Install GUMMY.command" <<'COMMAND'
#!/bin/bash
PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
package_status=0
bash "$PACKAGE_DIR/Scripts/install_mac.sh" --build-dir "$PACKAGE_DIR/build-mac" || package_status=$?
printf '\nPress Return to close this window.\n'
read -r package_reply || true
exit "$package_status"
COMMAND
cat > "$package_root/Validate GUMMY.command" <<'COMMAND'
#!/bin/bash
PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
package_status=0
bash "$PACKAGE_DIR/Scripts/validate_au.sh" || package_status=$?
printf '\nPress Return to close this window.\n'
read -r package_reply || true
exit "$package_status"
COMMAND
cat > "$package_root/Uninstall GUMMY.command" <<'COMMAND'
#!/bin/bash
PACKAGE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
package_status=0
bash "$PACKAGE_DIR/Scripts/uninstall_mac.sh" || package_status=$?
printf '\nPress Return to close this window.\n'
read -r package_reply || true
exit "$package_status"
COMMAND
chmod 755 "$package_root/"*.command
cat > "$package_root/START HERE.txt" <<'README'
GUMMY — macOS development build

1. Extract the entire GUMMY-macOS.zip archive. Keep the folder intact.
2. Quit Logic Pro and other audio hosts.
3. Double-click Install GUMMY.command. This copies the AU and VST3 to your
   own Library, without Xcode, CMake, or administrator privileges.
4. Double-click Validate GUMMY.command and check that validation passes.
5. Restart Logic. Open Settings > Plug-in Manager and confirm GUMMY is enabled.
6. On a track's Audio FX insert choose Audio Units > GUMMY Audio > GUMMY.

The optional Standalone application is in
build-mac/GUMMY_artefacts/Release/Standalone/GUMMY.app.
Uninstall GUMMY.command asks for confirmation and keeps your user presets.

This package is signed ad hoc for development, not Developer ID-signed or
Apple-notarized. Downloading it may trigger macOS security checks. If blocked,
follow Documentation/LOGIC_INSTALL.md; do not disable macOS security globally.
A successful CI build/auval run does not establish actual Logic compatibility;
verify playback, automation, GUI and project recall on your own Mac.
README
mkdir -p "$output_dir"
ditto -c -k --sequesterRsrc --keepParent "$package_root" "$staging_root/GUMMY-macOS.zip"
mv "$staging_root/GUMMY-macOS.zip" "$output_dir/GUMMY-macOS.zip"
(cd "$output_dir" && shasum -a 256 GUMMY-macOS.zip > GUMMY-macOS.zip.sha256)
printf 'Created: %s/GUMMY-macOS.zip\n' "$output_dir"
