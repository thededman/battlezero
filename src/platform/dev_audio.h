// Speaker output. The CardputerZero routes sound through PipeWire, which holds the codec,
// so the PulseAudio-compatible client library is tried first and plain ALSA second. Both
// are loaded at run time: the game still starts (silently) if neither is usable.
#pragma once
#include <dlfcn.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <thread>

#include "sound.h"

struct AudioOut {
    // The few ALSA calls needed, resolved from libasound.so.2.
    int (*pcm_open)(void**, const char*, int, int) = nullptr;
    int (*pcm_set_params)(void*, int, int, unsigned, unsigned, int, unsigned) = nullptr;
    long (*pcm_writei)(void*, const void*, unsigned long) = nullptr;
    int (*pcm_recover)(void*, int, int) = nullptr;
    int (*pcm_drop)(void*) = nullptr;
    int (*pcm_close)(void*) = nullptr;
    const char* (*strerror_)(int) = nullptr;

    // And the simple PulseAudio API from libpulse-simple.so.0.
    struct PaSpec {
        int format;
        uint32_t rate;
        uint8_t channels;
    };
    struct PaBufferAttr {
        uint32_t maxlength, tlength, prebuf, minreq, fragsize;
    };
    void* (*pa_new)(const char*, const char*, int, const char*, const char*, const PaSpec*, const void*,
                    const PaBufferAttr*, int*) = nullptr;
    int (*pa_write)(void*, const void*, size_t, int*) = nullptr;
    void (*pa_free)(void*) = nullptr;

    void* lib = nullptr;
    void* pcm = nullptr;
    void* pulse = nullptr;
    bz::Synth synth;
    std::mutex lock; // guards synth
    std::atomic<bool> stop{false};
    std::thread worker;

    template <typename F> bool sym(F& fn, const char* name) {
        fn = reinterpret_cast<F>(dlsym(lib, name));
        return fn != nullptr;
    }

    bool openPulse() {
        lib = dlopen("libpulse-simple.so.0", RTLD_NOW);
        if (!lib) return false;
        if (!sym(pa_new, "pa_simple_new") || !sym(pa_write, "pa_simple_write") || !sym(pa_free, "pa_simple_free"))
            return false;
        PaSpec spec = {3 /* signed 16-bit little-endian */, static_cast<uint32_t>(bz::SOUND_RATE), 1};
        // Ask for about 50 ms of buffering so effects land close to the action.
        PaBufferAttr attr = {~0u, static_cast<uint32_t>(bz::SOUND_RATE * 2 / 20), ~0u, ~0u, ~0u};
        int err = 0;
        pulse = pa_new(nullptr, "BattleZero", 1 /* playback */, nullptr, "game", &spec, nullptr, &attr, &err);
        if (!pulse) {
            std::fprintf(stderr, "sound server not available (code %d), trying ALSA\n", err);
            return false;
        }
        std::fprintf(stderr, "sound: system sound server, %d Hz mono\n", bz::SOUND_RATE);
        return true;
    }

    bool openAlsa() {
        lib = dlopen("libasound.so.2", RTLD_NOW);
        if (!lib) {
            std::fprintf(stderr, "sound off: %s\n", dlerror());
            return false;
        }
        if (!sym(pcm_open, "snd_pcm_open") || !sym(pcm_set_params, "snd_pcm_set_params") ||
            !sym(pcm_writei, "snd_pcm_writei") || !sym(pcm_recover, "snd_pcm_recover") ||
            !sym(pcm_drop, "snd_pcm_drop") || !sym(pcm_close, "snd_pcm_close") ||
            !sym(strerror_, "snd_strerror")) {
            std::fprintf(stderr, "sound off: ALSA library is missing expected functions\n");
            return false;
        }
        const char* name = std::getenv("BATTLEZERO_AUDIO");
        if (!name) name = "default";
        int err = pcm_open(&pcm, name, 0 /* playback */, 0);
        if (err < 0) {
            std::fprintf(stderr, "sound off: cannot open '%s': %s\n", name, strerror_(err));
            pcm = nullptr;
            return false;
        }
        // Signed 16-bit little-endian (format 2), interleaved (access 3), mono, resampling
        // allowed, about 60 ms of buffering.
        err = pcm_set_params(pcm, 2, 3, 1, bz::SOUND_RATE, 1, 60000);
        if (err < 0) {
            std::fprintf(stderr, "sound off: cannot set format on '%s': %s\n", name, strerror_(err));
            pcm_close(pcm);
            pcm = nullptr;
            return false;
        }
        std::fprintf(stderr, "sound: ALSA '%s', %d Hz mono\n", name, bz::SOUND_RATE);
        return true;
    }

    // BATTLEZERO_AUDIO names an ALSA device and skips the sound server.
    bool open() {
        if (!(std::getenv("BATTLEZERO_AUDIO") == nullptr && openPulse()) && !openAlsa()) return false;
        worker = std::thread([this] {
            int16_t buf[256];
            while (!stop) {
                {
                    std::lock_guard<std::mutex> g(lock);
                    synth.render(buf, 256);
                }
                // Both writes block until there is room, which paces this thread.
                if (pulse) {
                    int err = 0;
                    if (pa_write(pulse, buf, sizeof buf, &err) < 0) break;
                } else {
                    long n = pcm_writei(pcm, buf, 256);
                    if (n < 0 && pcm_recover(pcm, static_cast<int>(n), 1) < 0) break;
                }
            }
        });
        return true;
    }

    // Called once per frame from the game loop.
    void update(uint32_t sounds, float engine, bool muted) {
        std::lock_guard<std::mutex> g(lock);
        synth.trigger(sounds);
        synth.setEngine(engine);
        synth.muted = muted;
    }

    void close() {
        stop = true;
        if (worker.joinable()) worker.join();
        if (pulse) {
            pa_free(pulse);
            pulse = nullptr;
        }
        if (pcm) {
            pcm_drop(pcm);
            pcm_close(pcm);
            pcm = nullptr;
        }
    }
};
