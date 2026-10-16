#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=mac_common.sh
source "$SCRIPT_DIR/mac_common.sh"
case "${1:-}" in
    --help|-h) printf 'Usage: Scripts/uninstall_mac.sh\nRemoves identified GUMMY user-level AU/VST3 bundles after confirmation. Preserves presets and previous-version backups.\n'; exit 0 ;;
    '') ;;
    *) gummy_fail 'No options are accepted except --help.' ;;
esac
[ "$#" -le 1 ] || gummy_fail 'Unexpected arguments.'
gummy_require_mac
gummy_plist_tool
gummy_user_directories
# No installed plugin directories means there is nothing to remove.
if [ ! -e "$GUMMY_PLUGIN_ROOT" ]; then printf 'No user-level GUMMY installation found. Presets were preserved.\n'; exit 0; fi
gummy_lock
targets=()
for format in AU VST3; do
    if [ "$format" = AU ]; then parent="$GUMMY_AU_DIR"; name=GUMMY.component; else parent="$GUMMY_VST3_DIR"; name=GUMMY.vst3; fi
    [ -e "$parent" ] || continue
    [ "$(cd "$parent" && pwd -P)" = "$parent" ] || gummy_fail "Symbolic-link plugin directory refused: $parent"
    target="$parent/$name"
    if [ -e "$target" ] || [ -L "$target" ]; then
        gummy_verify_bundle "$target" "$format"
        targets+=("$target")
    fi
done
if [ "${#targets[@]}" -eq 0 ]; then printf 'No user-level GUMMY bundles found. Presets were preserved.\n'; exit 0; fi
printf 'Close Logic Pro and other audio hosts. These bundles will be removed:\n'
printf '  %s\n' "${targets[@]}"
printf 'User presets and .GUMMY-backups will be preserved.\nType REMOVE to continue: '
answer=''
IFS= read -r answer || true
if [ "$answer" != REMOVE ]; then printf '\nCancelled; no bundles removed.\n'; exit 0; fi
for target in "${targets[@]}"; do
    case "$target" in *.component) format=AU ;; *.vst3) format=VST3 ;; *) gummy_fail 'Unexpected uninstall path.' ;; esac
    gummy_verify_bundle "$target" "$format"
    rm -rf "$target"
    printf 'Removed: %s\n' "$target"
done
printf 'Uninstallation succeeded. Restart Logic Pro. User presets were preserved.\n'
