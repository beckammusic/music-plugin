#!/bin/bash
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=mac_common.sh
source "$SCRIPT_DIR/mac_common.sh"
bundle="${HOME:-}/Library/Audio/Plug-Ins/Components/GUMMY.component"
pluginval_path=''
usage() { printf 'Usage: Scripts/validate_au.sh [--bundle PATH] [--pluginval PATH]\nValidates the installed AU using identifiers read from its Info.plist.\n'; }
while [ "$#" -gt 0 ]; do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --bundle|--pluginval)
            [ "$#" -ge 2 ] && [ -n "$2" ] || gummy_fail "Missing value after $1"
            if [ "$1" = --bundle ]; then bundle="$2"; else pluginval_path="$2"; fi
            shift 2 ;;
        *) gummy_fail "Unknown option: $1 (use --help)" ;;
    esac
done
gummy_require_mac
gummy_plist_tool
gummy_need auval
gummy_need codesign
gummy_verify_bundle "$bundle" AU
codesign --verify --strict --verbose=2 "$bundle" || gummy_fail 'The AU has an invalid signature. Rebuild/sign the original bundle.'
# Read actual values. Never substitute a hard-coded code in the auval command.
component_type="$(gummy_plist_get "$bundle" AudioComponents:0:type)"
component_subtype="$(gummy_plist_get "$bundle" AudioComponents:0:subtype)"
component_manufacturer="$(gummy_plist_get "$bundle" AudioComponents:0:manufacturer)"
for code in "$component_type" "$component_subtype" "$component_manufacturer"; do
    [ "${#code}" -eq 4 ] || gummy_fail "Invalid four-character AU code in metadata: '$code'"
done
printf 'Bundle: %s\nMetadata identifiers: %s %s %s\n' "$bundle" "$component_type" "$component_subtype" "$component_manufacturer"
listing="$(auval -a)" || gummy_fail 'auval could not enumerate Audio Units.'
if ! printf '%s\n' "$listing" | awk -v t="$component_type" -v s="$component_subtype" -v m="$component_manufacturer" \
    '$1 == t && $2 == s && $3 == m { print; found = 1 } END { exit !found }'; then
    gummy_fail 'GUMMY is not discoverable by the Audio Unit registry. Install the AU, restart the audio host (or log out/in), then retry. See Documentation/LOGIC_INSTALL.md.'
fi
auval -v "$component_type" "$component_subtype" "$component_manufacturer" || gummy_fail 'Audio Unit validation failed. Save the auval output and investigate before using this build.'
printf 'Audio Unit discovery and auval validation passed.\n'
if [ -z "$pluginval_path" ]; then pluginval_path="$(command -v pluginval || true)"; fi
if [ -n "$pluginval_path" ]; then
    [ -x "$pluginval_path" ] || gummy_fail "pluginval is not executable: $pluginval_path"
    "$pluginval_path" --strictness-level 5 --validate "$bundle" || gummy_fail 'pluginval validation failed.'
    printf 'pluginval strictness level 5 passed.\n'
else
    printf 'pluginval was not found; its additional validation is pending.\n'
fi
printf 'Logic Pro loading, playback, automation, GUI and project recall must be checked separately.\n'
