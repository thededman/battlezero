#include "sound.h"

#include <cmath>

namespace bz {

void Synth::play(Wave w, float f0, float f1, float seconds, float volume, float delaySeconds) {
    Voice* slot = &voices[0];
    for (Voice& v : voices) {
        if (!v.active) {
            slot = &v;
            break;
        }
        // All busy: replace whichever is nearest its end.
        if (v.length - v.pos < slot->length - slot->pos) slot = &v;
    }
    *slot = Voice();
    slot->active = true;
    slot->wave = w;
    slot->f0 = f0;
    slot->f1 = f1;
    slot->volume = volume;
    slot->delay = static_cast<int>(delaySeconds * SOUND_RATE);
    slot->length = static_cast<int>(seconds * SOUND_RATE);
}

void Synth::trigger(uint32_t s) {
    if (s & SND_FIRE) play(SQUARE, 950, 180, 0.18f, 0.5f);
    if (s & SND_ENEMY_FIRE) play(SAW, 420, 110, 0.26f, 0.45f);
    if (s & SND_TELL) play(SQUARE, 1400, 1400, 0.05f, 0.25f);
    if (s & SND_BUMP) play(NOISE, 0, 0, 0.08f, 0.3f);
    if (s & SND_KILL) {
        play(NOISE, 0, 0, 0.6f, 0.6f);
        play(SQUARE, 220, 45, 0.5f, 0.4f);
    }
    if (s & SND_HIT) {
        play(NOISE, 0, 0, 1.1f, 0.9f);
        play(SAW, 95, 35, 1.0f, 0.6f);
    }
    if (s & SND_WAVE) {
        play(SQUARE, 660, 660, 0.09f, 0.35f);
        play(SQUARE, 880, 880, 0.14f, 0.35f, 0.12f);
    }
    if (s & SND_GAME_OVER) {
        play(SQUARE, 440, 330, 0.3f, 0.45f);
        play(SQUARE, 330, 220, 0.3f, 0.45f, 0.32f);
        play(SQUARE, 220, 82, 0.7f, 0.45f, 0.64f);
    }
}

void Synth::setEngine(float level) { engineLevel = level < 0 ? 0 : (level > 1 ? 1 : level); }

bool Synth::silent() const {
    if (engineLevel > 0 || engineNow > 0.001f) return false;
    for (const Voice& v : voices)
        if (v.active) return false;
    return true;
}

void Synth::render(int16_t* out, int count) {
    for (int i = 0; i < count; ++i) {
        float mix = 0;
        for (Voice& v : voices) {
            if (!v.active) continue;
            if (v.delay > 0) {
                --v.delay;
                continue;
            }
            float t = static_cast<float>(v.pos) / static_cast<float>(v.length);
            float env = 1.0f - t; // straight fade to silence
            float s;
            if (v.wave == NOISE) {
                // Stepped noise that slows as it fades gives a rumble rather than a hiss.
                int hold = 2 + static_cast<int>(t * 10);
                if (v.pos % hold == 0) {
                    noise = noise * 1664525u + 1013904223u;
                    v.phase = static_cast<float>((noise >> 16) & 0xFFFF) / 32768.0f - 1.0f;
                }
                s = v.phase;
            } else {
                float f = v.f0 * std::pow(v.f1 / v.f0, t);
                v.phase += f / SOUND_RATE;
                if (v.phase >= 1) v.phase -= 1;
                s = v.wave == SQUARE ? (v.phase < 0.5f ? 1.0f : -1.0f) : 2 * v.phase - 1;
            }
            mix += s * env * v.volume;
            if (++v.pos >= v.length) v.active = false;
        }

        // Engine: a low saw whose pitch and loudness follow the speed.
        engineNow += (engineLevel - engineNow) * 0.0008f;
        if (engineNow > 0.001f) {
            enginePhase += (48.0f + 50.0f * engineNow) / SOUND_RATE;
            if (enginePhase >= 1) enginePhase -= 1;
            mix += (2 * enginePhase - 1) * (0.05f + 0.10f * engineNow);
        }

        float o = muted ? 0 : mix * master;
        o = o > 1 ? 1 : (o < -1 ? -1 : o);
        out[i] = static_cast<int16_t>(o * 32000);
    }
}

} // namespace bz
