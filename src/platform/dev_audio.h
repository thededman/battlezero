// Speaker output through ALSA. The ALSA library is loaded at run time, so the game still
// starts (silently) on a system without it, or when the sound device is busy.
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

    void* lib = nullptr;
    void* pcm = nullptr;
    bz::Synth synth;
    std::mutex lock; // guards synth
    std::atomic<bool> stop{false};
    std::thread worker;

    template <typename F> bool sym(F& fn, const char* name) {
        fn = reinterpret_cast<F>(dlsym(lib, name));
        return fn != nullptr;
    }

    bool open() {
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
        worker = std::thread([this] {
            int16_t buf[256];
            while (!stop) {
                {
                    std::lock_guard<std::mutex> g(lock);
                    synth.render(buf, 256);
                }
                long n = pcm_writei(pcm, buf, 256); // blocks until there is room
                if (n < 0 && pcm_recover(pcm, static_cast<int>(n), 1) < 0) break;
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
        if (pcm) {
            pcm_drop(pcm);
            pcm_close(pcm);
            pcm = nullptr;
        }
    }
};
