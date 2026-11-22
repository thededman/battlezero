// Sound effects, synthesised: no sample files. A front end calls render() to fill its
// audio buffer. Not thread-safe: the front end guards trigger/render with its own lock.
#pragma once
#include <cstdint>

namespace bz {

constexpr int SOUND_RATE = 22050; // mono, signed 16-bit

enum Sound : uint32_t {
    SND_FIRE = 1u << 0,       // the player's shot
    SND_ENEMY_FIRE = 1u << 1,
    SND_KILL = 1u << 2,       // an enemy breaks up
    SND_HIT = 1u << 3,        // the player is hit
    SND_WAVE = 1u << 4,       // a new wave arrives
    SND_GAME_OVER = 1u << 5,
    SND_TELL = 1u << 6,       // an enemy starts winding up a shot
    SND_BUMP = 1u << 7,       // drove into something
};

struct Synth {
    enum Wave { SQUARE, SAW, NOISE };
    struct Voice {
        bool active = false;
        Wave wave = SQUARE;
        float f0 = 0, f1 = 0;   // frequency sweeps from f0 to f1
        float volume = 0;
        int delay = 0, length = 0, pos = 0; // in samples
        float phase = 0;
    };

    void trigger(uint32_t sounds); // any combination of Sound bits
    void setEngine(float level);   // 0 = off, otherwise 0..1 with speed
    void render(int16_t* out, int count);
    bool silent() const;

    bool muted = false;
    float master = 0.6f;

private:
    void play(Wave w, float f0, float f1, float seconds, float volume, float delaySeconds = 0);

    Voice voices[12];
    float engineLevel = 0, engineNow = 0, enginePhase = 0;
    uint32_t noise = 0x1234567u;
};

} // namespace bz
