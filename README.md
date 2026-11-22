# BattleZero

A first-person wireframe arena shooter for the M5Stack CardputerZero. You drive a
CardputerZero across a flat plain of oversized hardware and fight off the older
Cardputer models. Tilt the device left and right to steer; keys drive and fire.

BattleZero is an original game. It is not affiliated with M5Stack.

## Controls

| Action | Device | Simulator (Mac) |
|---|---|---|
| Steer | tilt left / right, or Z / C | Z / C or arrow keys; Q / E hold a fake tilt |
| Forward / reverse | F / X | F / X or up / down |
| Fire | Space or Enter | Space or Enter |
| Recentre tilt | R | R |
| Tilt on / off | T | T |
| Pause, then quit | ESC | ESC |

## Build

Simulator and tests on a Mac (needs CMake and SDL2):

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build --output-on-failure
```

```bash
./build/battlezero-sim
```

Device build and install: see `packaging/README.md`.

## Layout

- `src/core/` the game, with no OS calls
- `src/platform/` simulator (SDL2) and device (framebuffer, evdev, IIO) front ends
- `tests/` core tests, run by CTest
- `packaging/` `.deb` build and deploy scripts
- `tools/probe.sh` read-only hardware check for a CardputerZero

## How a change goes out

One issue per bug or request, one pull request (`Closes #n`), one changelog entry, one
tag. David's approval is recorded on the pull request before it is merged.
