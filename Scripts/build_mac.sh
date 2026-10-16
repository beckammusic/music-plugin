#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=mac_common.sh
source "$SCRIPT_DIR/mac_common.sh"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
build_dir="$PROJECT_DIR/build-mac"
juce_path="${GUMMY_JUCE_PATH:-}"
architecture=native
jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '2')"
sign_identity=-

usage() {
    cat <<'EOF'
Usage: Scripts/build_mac.sh [options]
  --juce PATH       Optional local JUCE 8.0.6 checkout (or GUMMY_JUCE_PATH).
  --build-dir PATH  Build directory; default: project/build-mac.
  --arch ARCH       native (default), arm64, x86_64, or universal.
  --jobs NUMBER     Parallel compiler jobs; lower this if memory is limited.
  --sign IDENTITY   codesign identity; default '-' uses local ad-hoc signing.
  --help            Show this help.
Builds Release AU, VST3, and Standalone products with Xcode's macOS toolchain.
Runs the automated tests before signing. CMake fetches pinned JUCE if absent.
EOF
}
while [ "$#" -gt 0 ]; do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --juce|--build-dir|--arch|--jobs|--sign)
            [ "$#" -ge 2 ] && [ -n "$2" ] || gummy_fail "Missing value after $1"
            case "$1" in
                --juce) juce_path="$2" ;;
                --build-dir) build_dir="$2" ;;
                --arch) architecture="$2" ;;
                --jobs) jobs="$2" ;;
                --sign) sign_identity="$2" ;;
            esac
            shift 2 ;;
        *) gummy_fail "Unknown option: $1 (use --help)" ;;
    esac
done
gummy_require_mac
for required in cmake ctest xcode-select xcrun codesign; do gummy_need "$required"; done
xcode-select -p >/dev/null 2>&1 || gummy_fail 'Install Xcode or run xcode-select --install, then retry.'
xcrun --find clang++ >/dev/null 2>&1 || gummy_fail 'Apple clang++ is unavailable. Check xcode-select and Xcode license acceptance.'
xcrun --show-sdk-path >/dev/null 2>&1 || gummy_fail 'The selected Xcode toolchain has no macOS SDK.'
case "$jobs" in ''|*[!0-9]*|0) gummy_fail '--jobs must be a positive integer.' ;; esac
case "$architecture" in
    native) architecture="$(uname -m)" ;;
    universal) architecture='arm64;x86_64' ;;
    arm64|x86_64) ;;
    *) gummy_fail 'Use --arch native, arm64, x86_64, or universal.' ;;
esac
case "$architecture" in arm64|x86_64|'arm64;x86_64') ;; *) gummy_fail "Unsupported macOS architecture: $architecture" ;; esac
if [ -z "$juce_path" ]; then
    for candidate in "$PROJECT_DIR/../deps/JUCE" "$PROJECT_DIR/External/JUCE" "$PROJECT_DIR/JUCE"; do
        if [ -f "$candidate/CMakeLists.txt" ]; then juce_path="$candidate"; break; fi
    done
fi
if [ -n "$juce_path" ]; then
    [ -f "$juce_path/CMakeLists.txt" ] || gummy_fail "JUCE source not found at $juce_path"
    juce_path="$(cd "$juce_path" && pwd -P)"
    grep -Eq 'project\(JUCE VERSION 8\.0\.6([[:space:]]|\))' "$juce_path/CMakeLists.txt" || gummy_fail "Expected pinned JUCE 8.0.6 at $juce_path"
    printf 'Configuring GUMMY Release for %s using %s\n' "$architecture" "$juce_path"
else
    printf 'Configuring GUMMY Release for %s; CMake will fetch pinned JUCE 8.0.6 (network access required on first configuration).\n' "$architecture"
fi
cmake -S "$PROJECT_DIR" -B "$build_dir" -G Xcode \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED=NO \
    "-DCMAKE_OSX_ARCHITECTURES=$architecture" \
    "-DGUMMY_JUCE_PATH=$juce_path" \
    -DGUMMY_BUILD_PLUGIN=ON \
    -DGUMMY_BUILD_TESTS=ON \
    -DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF
cmake --build "$build_dir" --config Release \
    --target GUMMY_AU GUMMY_VST3 GUMMY_Standalone \
    GummyDSPTests GummyPluginTests GummyFormatTests --parallel "$jobs"
ctest --test-dir "$build_dir" -C Release --output-on-failure

for relative in 'AU/GUMMY.component' 'VST3/GUMMY.vst3' 'Standalone/GUMMY.app'; do
    bundle="$build_dir/GUMMY_artefacts/Release/$relative"
    [ -d "$bundle" ] || gummy_fail "Build completed without expected product: $bundle"
    if [ "$sign_identity" = - ]; then
        codesign --force --sign - --timestamp=none "$bundle"
    else
        codesign --force --sign "$sign_identity" --options runtime --timestamp "$bundle"
    fi
    codesign --verify --strict --verbose=2 "$bundle"
    printf 'Built and signature checked: %s\n' "$bundle"
done
printf '\nBuild succeeded. Next: Scripts/install_mac.sh --build-dir "%s"\n' "$build_dir"
printf 'AU validation and a Logic Pro playback test are still required on this Mac.\n'
