#!/bin/bash
# Installs the .pkg through the scenarios users hit and checks the result. Needs sudo, and
# removes any Glacier Riser already installed system-wide, so run it on CI or a test Mac.
#   sudo packaging/macos/test-pkg.sh dist/GlacierRiser-macOS-Installer.pkg
# Leaves every format installed; then, as a normal user (AU registration is per login session):
#   auval -v aumf Glrs Icsd
set -euo pipefail
PKG=$1
VST3="/Library/Audio/Plug-Ins/VST3/Glacier Riser.vst3"
AU="/Library/Audio/Plug-Ins/Components/Glacier Riser.component"
APP="/Applications/Glacier Riser.app"
ALL=("$VST3" "$AU" "$APP")

fail() { echo "FAIL: $*"; exit 1; }
pass() { echo "ok - $*"; }
clean() {
  rm -rf "${ALL[@]}"
  for id in $(pkgutil --pkgs | grep com.icecoldsound.glacierriser || true); do pkgutil --forget "$id" >/dev/null; done
}
install() { installer -pkg "$PKG" -target / "$@" >/dev/null || fail "installer exited non-zero"; }
expect() { # <present...> -- <absent...>
  local absent=0
  for p in "$@"; do
    [ "$p" = -- ] && { absent=1; continue; }
    if [ $absent = 0 ]; then [ -d "$p" ] || fail "missing $p"; else [ ! -e "$p" ] || fail "should not exist: $p"; fi
  done
}
choices() { # deselect every choice id passed
  local f; f=$(mktemp)
  { echo '<?xml version="1.0" encoding="UTF-8"?><plist version="1.0"><array>'
    for id in "$@"; do echo "<dict><key>choiceIdentifier</key><string>$id</string><key>choiceAttribute</key><string>selected</string><key>attributeSetting</key><integer>0</integer></dict>"; done
    echo '</array></plist>'; } > "$f"
  echo "$f"
}

# Payload sanity: right install locations, bundles only, three choices.
installer -pkg "$PKG" -showChoicesXML -target / | grep -c '<string>\(vst3\|au\|standalone\)</string>' | grep -q 3 || fail "expected 3 choices"
pass "pkg offers VST3, AU and Standalone"

# 1. Fresh install, everything (the default).
clean; install
expect "${ALL[@]}"
for b in "${ALL[@]}"; do
  codesign --verify --deep --strict "$b" || fail "signature broken after install: $b"
  [ -z "$(xattr -r "$b" 2>/dev/null | grep quarantine)" ] || fail "quarantine flag on $b"
  [ -z "$(find "$b" -name '._*')" ] || fail "AppleDouble files in $b"
  lipo -archs "$(find "$b/Contents/MacOS" -type f | head -1)" | grep -q "x86_64 arm64" || fail "not universal: $b"
done
[ "$(stat -f %Su "$VST3")" = root ] || fail "VST3 not owned by root"
pass "fresh install: all three formats, signed, universal, no quarantine"

# 2. Reinstall over the top (an update).
install; expect "${ALL[@]}"
pass "reinstall over an existing install"

# 3. AU only (Logic user who unticks the rest).
clean; install -applyChoiceChangesXML "$(choices vst3 standalone)"
expect "$AU" -- "$VST3" "$APP"
pass "AU-only install leaves VST3 and the app out"

# 4. VST3 + Standalone, no AU.
clean; install -applyChoiceChangesXML "$(choices au)"
expect "$VST3" "$APP" -- "$AU"
pass "install without AU"

# 5. An older copy that was moved elsewhere must not swallow the update.
clean; install
mkdir -p /tmp/gr-moved && mv "$APP" /tmp/gr-moved/
install
expect "$APP"
rm -rf /tmp/gr-moved
pass "app reinstalled to /Applications even when an old copy lives elsewhere"

echo "All installer scenarios passed."
