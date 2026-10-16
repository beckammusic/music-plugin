#!/bin/bash
# Shared checks for the macOS scripts. Compatible with macOS /bin/bash 3.2.

gummy_fail() { printf 'GUMMY: %s\n' "$*" >&2; exit 1; }
gummy_need() { command -v "$1" >/dev/null 2>&1 || gummy_fail "Required command not found: $1"; }
gummy_require_mac() {
    [ "$(uname -s)" = Darwin ] || gummy_fail 'This script requires macOS; no files were changed.'
}
gummy_plist_tool() {
    GUMMY_PLIST_BUDDY="${GUMMY_PLIST_BUDDY:-/usr/libexec/PlistBuddy}"
    [ -x "$GUMMY_PLIST_BUDDY" ] || gummy_fail "PlistBuddy is not executable: $GUMMY_PLIST_BUDDY"
}
gummy_plist_get() { "$GUMMY_PLIST_BUDDY" -c "Print :$2" "$1/Contents/Info.plist" 2>/dev/null; }

# JUCE 8.0.6 uses the configured BUNDLE_ID unchanged for AU and VST3.
gummy_verify_bundle() {
    local bundle="$1" format="$2" identifier executable
    [ -d "$bundle" ] && [ ! -L "$bundle" ] || gummy_fail "Missing bundle or symbolic-link bundle refused: $bundle"
    [ -f "$bundle/Contents/Info.plist" ] && [ ! -L "$bundle/Contents/Info.plist" ] || gummy_fail "Missing or symbolic-link Info.plist: $bundle"
    identifier="$(gummy_plist_get "$bundle" CFBundleIdentifier)" || gummy_fail "Cannot read bundle identifier: $bundle"
    [ "$identifier" = com.gummyaudio.gummy ] || gummy_fail "Refusing a bundle with unrelated identifier '$identifier': $bundle"
    executable="$(gummy_plist_get "$bundle" CFBundleExecutable)" || gummy_fail "Cannot read executable name: $bundle"
    [ "$executable" = GUMMY ] || gummy_fail "Unexpected bundle executable '$executable': $bundle"
    [ -f "$bundle/Contents/MacOS/GUMMY" ] && [ ! -L "$bundle/Contents/MacOS/GUMMY" ] && [ -x "$bundle/Contents/MacOS/GUMMY" ] || gummy_fail "Missing executable GUMMY binary or symbolic-link executable: $bundle"
    [ "$(gummy_plist_get "$bundle" CFBundleName)" = GUMMY ] || gummy_fail "Unexpected product name: $bundle"
    if [ "$format" = AU ]; then
        [ "$(gummy_plist_get "$bundle" AudioComponents:0:type)" = aufx ] &&
        [ "$(gummy_plist_get "$bundle" AudioComponents:0:subtype)" = Gmmy ] &&
        [ "$(gummy_plist_get "$bundle" AudioComponents:0:manufacturer)" = GmAu ] || gummy_fail "Audio Unit identity does not match GUMMY: $bundle"
    fi
}

gummy_user_directories() {
    [ -n "${HOME:-}" ] && [ -d "$HOME" ] || gummy_fail 'HOME must name an existing user directory.'
    [ "$(id -u)" != 0 ] || gummy_fail 'Run as your regular macOS user, without sudo.'
    GUMMY_USER_HOME="$(cd "$HOME" && pwd -P)"
    GUMMY_PLUGIN_ROOT="$GUMMY_USER_HOME/Library/Audio/Plug-Ins"
    GUMMY_AU_DIR="$GUMMY_PLUGIN_ROOT/Components"
    GUMMY_VST3_DIR="$GUMMY_PLUGIN_ROOT/VST3"
}
gummy_safe_directory() {
    local directory="$1" ancestor="$1"
    # Check existing ancestors before mkdir, so a redirected Library cannot
    # cause even intermediate directories to be created outside the user path.
    while [ ! -d "$ancestor" ]; do
        [ ! -e "$ancestor" ] && [ ! -L "$ancestor" ] || gummy_fail "Invalid plugin directory ancestor: $ancestor"
        ancestor="${ancestor%/*}"
    done
    [ "$(cd "$ancestor" && pwd -P)" = "$ancestor" ] || gummy_fail "Refusing a plugin directory reached through a symbolic link: $directory"
    mkdir -p "$directory" || gummy_fail "Cannot create user plugin directory: $directory"
    [ "$(cd "$directory" && pwd -P)" = "$directory" ] || gummy_fail "Refusing a plugin directory reached through a symbolic link: $directory"
}
gummy_lock() {
    gummy_safe_directory "$GUMMY_PLUGIN_ROOT"
    GUMMY_LOCK="$GUMMY_PLUGIN_ROOT/.gummy-maintenance.lock"
    mkdir "$GUMMY_LOCK" 2>/dev/null || gummy_fail "Another GUMMY install/uninstall may be running ($GUMMY_LOCK). If none is running, remove this empty lock directory and retry."
    trap 'rmdir "$GUMMY_LOCK" 2>/dev/null || true' EXIT
    trap 'exit 130' INT
    trap 'exit 143' TERM
}
