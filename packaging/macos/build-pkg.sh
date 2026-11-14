#!/bin/bash
# Builds the macOS installer: one .pkg with VST3, AU and Standalone choices, all on by default.
#   packaging/macos/build-pkg.sh <artefacts-dir> <version> <out.pkg>
# <artefacts-dir> holds VST3/, AU/ and Standalone/ (e.g. Builds/GlacierRiser_artefacts/Release).
# Unsigned for now. signing: pass --sign "Developer ID Installer: ..." to productbuild, then notarize.
set -euo pipefail

ARTEFACTS=$1 VERSION=$2 OUT=$3
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

ID=com.icecoldsound.glacierriser
component() { # <format> <bundle> <install-location> [scripts-dir]
  local root="$WORK/root-$1"
  mkdir -p "$root"
  ditto --norsrc --noextattr --noacl "$ARTEFACTS/$1/$2" "$root/$2"
  # Install exactly where we say, even if an older copy sits somewhere else on disk.
  pkgbuild --analyze --root "$root" "$WORK/$1.plist" >/dev/null
  plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$1.plist"
  pkgbuild --root "$root" --component-plist "$WORK/$1.plist" \
    --identifier "$ID.$(echo "$1" | tr '[:upper:]' '[:lower:]')" --version "$VERSION" \
    --install-location "$3" ${4:+--scripts "$4"} "$WORK/$1.pkg" >/dev/null
}

component VST3 "Glacier Riser.vst3" /Library/Audio/Plug-Ins/VST3
component AU "Glacier Riser.component" /Library/Audio/Plug-Ins/Components "$HERE/scripts-au"
component Standalone "Glacier Riser.app" /Applications

sed "s/@VERSION@/$VERSION/g" "$HERE/distribution.xml" > "$WORK/distribution.xml"
mkdir -p "$(dirname "$OUT")"
productbuild --distribution "$WORK/distribution.xml" --resources "$HERE/resources" \
  --package-path "$WORK" "$OUT"
echo "Built $OUT"
