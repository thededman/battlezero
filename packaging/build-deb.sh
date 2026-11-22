#!/usr/bin/env bash
# Builds the CardputerZero package (arm64 .deb) inside a Debian container and leaves it
# in dist/. Needs Docker. On an Apple Silicon Mac the container runs natively.
set -euo pipefail
cd "$(dirname "$0")/.."

docker run --rm --platform linux/arm64 -v "$PWD":/src -w /src debian:bookworm bash -euo pipefail -c '
  apt-get update -qq
  apt-get install -y -qq --no-install-recommends g++ cmake make dpkg-dev >/dev/null
  cmake -S . -B build-device -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build build-device -j4
  ctest --test-dir build-device --output-on-failure
  packaging/make-deb.sh build-device
'
ls -l dist/*.deb
