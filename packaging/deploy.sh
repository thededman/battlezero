#!/usr/bin/env bash
# Copies the newest package to a CardputerZero over SSH and installs it.
#   packaging/deploy.sh user@host
set -euo pipefail
cd "$(dirname "$0")/.."
target="${1:?usage: packaging/deploy.sh user@host}"
pkg="$(ls -t dist/battlezero_*_arm64.deb 2>/dev/null | head -n 1)"
[ -n "$pkg" ] || { echo "no package in dist/: run packaging/build-deb.sh first" >&2; exit 1; }
scp "$pkg" "$target:/tmp/"
ssh -t "$target" "sudo apt-get install -y --reinstall /tmp/$(basename "$pkg")"
echo "installed $(basename "$pkg") on $target"
