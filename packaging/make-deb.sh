#!/usr/bin/env bash
# Assembles the .deb from an existing Linux arm64 build directory.
#   packaging/make-deb.sh <build-dir>
set -euo pipefail
cd "$(dirname "$0")/.."
build="${1:?build directory}"
version="$(sed -n 's/^project(battlezero VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
chmod 755 "$stage"

install -D -m 755 "$build/battlezero" "$stage/usr/bin/battlezero"
install -D -m 644 packaging/battlezero.desktop "$stage/usr/share/APPLaunch/applications/battlezero.desktop"
install -D -m 644 packaging/battlezero.png "$stage/usr/share/APPLaunch/share/images/battlezero.png"
install -D -m 644 README.md "$stage/usr/share/doc/battlezero/README.md"
install -D -m 644 CHANGELOG.md "$stage/usr/share/doc/battlezero/CHANGELOG.md"

mkdir -p "$stage/DEBIAN"
cat > "$stage/DEBIAN/control" <<CONTROL
Package: battlezero
Version: $version
Section: games
Priority: optional
Architecture: arm64
Depends: libc6
Maintainer: David <thededman@users.noreply.github.com>
Description: Wireframe arena shooter for the CardputerZero
 Drive a CardputerZero across a wireframe plain and fight off the older
 Cardputer models. Tilt the device to steer.
CONTROL

mkdir -p dist
out="dist/battlezero_${version}_arm64.deb"
dpkg-deb --build --root-owner-group "$stage" "$out" >/dev/null
echo "built $out"
