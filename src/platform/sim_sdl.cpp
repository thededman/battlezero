// Mac simulator: the same game in an SDL2 window at 3x.
//   Z/C or left/right steer, F/X or up/down drive, Space/Enter fire, ESC pause.
//   --tilt pretends a motion sensor is fitted; Q and E then tilt left and right.
#include <SDL.h>
#include <cstring>

#include "game.h"

using namespace bz;

static Frame frame;
static Synth synth;

static void audioCallback(void*, Uint8* stream, int len) {
    synth.render(reinterpret_cast<int16_t*>(stream), len / 2);
}

int main(int argc, char** argv) {
    bool fakeTilt = false;
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--tilt")) fakeTilt = true;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    const int scale = 3;
    SDL_Window* win = SDL_CreateWindow("BattleZero simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       SCREEN_W * scale, SCREEN_H * scale, 0);
    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_Texture* tex =
        SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING, SCREEN_W, SCREEN_H);
    if (!win || !ren || !tex) {
        SDL_Log("SDL setup failed: %s", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want{}, have{};
    want.freq = SOUND_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = audioCallback;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (dev) SDL_PauseAudioDevice(dev, 0);
    else SDL_Log("no sound: %s", SDL_GetError());

    Game game(static_cast<uint32_t>(SDL_GetPerformanceCounter()));
    Uint64 last = SDL_GetPerformanceCounter();
    float fakeAngle = 0;
    bool running = true;
    while (running && !game.wantsQuit) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev))
            if (ev.type == SDL_QUIT) running = false;

        const Uint8* k = SDL_GetKeyboardState(nullptr);
        Input in;
        in.left = k[SDL_SCANCODE_Z] || k[SDL_SCANCODE_LEFT];
        in.right = k[SDL_SCANCODE_C] || k[SDL_SCANCODE_RIGHT];
        in.forward = k[SDL_SCANCODE_F] || k[SDL_SCANCODE_UP];
        in.reverse = k[SDL_SCANCODE_X] || k[SDL_SCANCODE_DOWN];
        in.fire = k[SDL_SCANCODE_SPACE] || k[SDL_SCANCODE_RETURN];
        in.pause = k[SDL_SCANCODE_ESCAPE];
        in.recenter = k[SDL_SCANCODE_R];
        in.tiltToggle = k[SDL_SCANCODE_T];
        in.invert = k[SDL_SCANCODE_I];
        in.axis = k[SDL_SCANCODE_A];
        in.mute = k[SDL_SCANCODE_M];

        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>(now - last) / static_cast<float>(SDL_GetPerformanceFrequency());
        last = now;

        if (fakeTilt) {
            float want = (k[SDL_SCANCODE_E] ? 0.4f : 0.0f) - (k[SDL_SCANCODE_Q] ? 0.4f : 0.0f);
            fakeAngle += (want - fakeAngle) * (dt * 6 > 1 ? 1 : dt * 6);
            in.tiltAvailable = true;
            // Matches the default direction setting: right edge down reads negative x.
            in.accel[0] = -std::sin(fakeAngle);
            in.accel[1] = 0.4f;
            in.accel[2] = std::cos(fakeAngle);
        }

        game.update(in, dt);
        if (dev) {
            SDL_LockAudioDevice(dev);
            synth.trigger(game.sounds);
            synth.setEngine(game.engine);
            synth.muted = game.muted;
            SDL_UnlockAudioDevice(dev);
        }
        game.sounds = 0;
        game.render(frame);
        SDL_UpdateTexture(tex, nullptr, frame.px, SCREEN_W * sizeof(Color));
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, tex, nullptr, nullptr);
        SDL_RenderPresent(ren);
    }
    SDL_Quit();
    return 0;
}
