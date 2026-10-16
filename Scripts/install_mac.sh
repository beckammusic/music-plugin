#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=mac_common.sh
source "$SCRIPT_DIR/mac_common.sh"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd -P)"
build_dir="$PROJECT_DIR/build-mac"
usage() { printf 'Usage: Scripts/install_mac.sh [--build-dir PATH]\nInstalls both Release bundles in your user Library. Close all audio hosts first.\n'; }
while [ "$#" -gt 0 ]; do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --build-dir) [ "$#" -ge 2 ] && [ -n "$2" ] || gummy_fail 'Missing --build-dir value'; build_dir="$2"; shift 2 ;;
        *) gummy_fail "Unknown option: $1 (use --help)" ;;
    esac
done
gummy_require_mac
gummy_plist_tool
gummy_need ditto
gummy_need codesign
gummy_user_directories
au_source="$build_dir/GUMMY_artefacts/Release/AU/GUMMY.component"
vst3_source="$build_dir/GUMMY_artefacts/Release/VST3/GUMMY.vst3"
# Check both formats before changing any installed product.
gummy_verify_bundle "$au_source" AU
gummy_verify_bundle "$vst3_source" VST3
codesign --verify --strict "$au_source" || gummy_fail 'AU signature verification failed. Rebuild/sign before installing.'
codesign --verify --strict "$vst3_source" || gummy_fail 'VST3 signature verification failed. Rebuild/sign before installing.'
gummy_lock
gummy_safe_directory "$GUMMY_AU_DIR"
gummy_safe_directory "$GUMMY_VST3_DIR"
for format in AU VST3; do
    if [ "$format" = AU ]; then target="$GUMMY_AU_DIR/GUMMY.component"; else target="$GUMMY_VST3_DIR/GUMMY.vst3"; fi
    if [ -e "$target" ] || [ -L "$target" ]; then gummy_verify_bundle "$target" "$format"; fi
done

install_one() {
    local source_bundle="$1" parent="$2" format="$3" name target stage backup=''
    name="${source_bundle##*/}"
    target="$parent/$name"
    stage="$(mktemp -d "$parent/.gummy-install.XXXXXX")" || gummy_fail "Cannot stage bundle in $parent"
    if ! ditto "$source_bundle" "$stage/$name"; then
        rm -rf "$stage"
        gummy_fail "Copy failed; installed bundle was preserved: $target"
    fi
    gummy_verify_bundle "$stage/$name" "$format"
    if ! codesign --verify --strict "$stage/$name"; then
        rm -rf "$stage"
        gummy_fail "Staged signature verification failed; installed bundle was preserved: $target"
    fi
    if [ -e "$target" ] || [ -L "$target" ]; then
        gummy_verify_bundle "$target" "$format"
        gummy_safe_directory "$parent/.GUMMY-backups"
        backup="$(mktemp -d "$parent/.GUMMY-backups/replaced.XXXXXX")" || gummy_fail 'Cannot create backup directory.'
        mv "$target" "$backup/$name" || gummy_fail "Cannot preserve previous bundle: $target"
    fi
    if ! mv "$stage/$name" "$target"; then
        if [ -n "$backup" ]; then
            mv "$backup/$name" "$target" || gummy_fail "Install failed and automatic restore failed. Previous bundle is at $backup/$name"
            rmdir "$backup" || true
        fi
        rm -rf "$stage"
        gummy_fail "Install failed: $target"
    fi
    rmdir "$stage"
    printf 'Installed: %s\n' "$target"
    if [ -n "$backup" ]; then printf 'Previous GUMMY bundle preserved: %s/%s\n' "$backup" "$name"; fi
}
install_one "$au_source" "$GUMMY_AU_DIR" AU
install_one "$vst3_source" "$GUMMY_VST3_DIR" VST3
printf '\nInstallation succeeded. Restart Logic Pro, then run Scripts/validate_au.sh.\n'
printf 'User presets were preserved. No system-wide plugin directory was modified.\n'
