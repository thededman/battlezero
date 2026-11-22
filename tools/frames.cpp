// Runs a scripted, deterministic session with no window and writes chosen frames as PPM
// files (3x scale), so screens can be checked without a device or a display.
//   bz-frames <output-dir>
#include <cstdio>
#include <string>

#include "game.h"

using namespace bz;

static Frame frame;

static void save(const Game& g, const std::string& dir, const char* name) {
    g.render(frame);
    const int S = 3;
    std::string path = dir + "/" + name + ".ppm";
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        std::perror(path.c_str());
        return;
    }
    std::fprintf(fp, "P6\n%d %d\n255\n", SCREEN_W * S, SCREEN_H * S);
    for (int y = 0; y < SCREEN_H * S; ++y)
        for (int x = 0; x < SCREEN_W * S; ++x) {
            Color c = frame.at(x / S, y / S);
            unsigned char rgb8[3] = {static_cast<unsigned char>(((c >> 11) & 31) * 255 / 31),
                                     static_cast<unsigned char>(((c >> 5) & 63) * 255 / 63),
                                     static_cast<unsigned char>((c & 31) * 255 / 31)};
            std::fwrite(rgb8, 1, 3, fp);
        }
    std::fclose(fp);
    std::printf("wrote %s\n", path.c_str());
}

static void run(Game& g, const Input& in, float seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 60); ++i) g.update(in, STEP);
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : ".";
    Game g(42);
    g.highScore = 12000;
    Input none, fire, tilt;
    fire.fire = true;
    tilt.tiltAvailable = true;

    run(g, none, 1.3f);
    save(g, dir, "1-title");

    // Calibration screen (as on the device, with a sensor present).
    tilt.accel[1] = -0.2f;
    tilt.fire = true;
    g.update(tilt, STEP);
    tilt.fire = false;
    run(g, tilt, 0.5f);
    save(g, dir, "2-calibrate");
    tilt.accel[1] = 0;
    tilt.fire = true;
    g.update(tilt, STEP);
    tilt.fire = false;

    // Wave 1 arrives; stage the Cardputer close in front for a clear look.
    run(g, tilt, 1.6f);
    if (!g.enemies.empty()) {
        Enemy& e = g.enemies[0];
        e.x = g.px + 6;
        e.z = g.pz + 45;
        e.heading = PI + 0.5f;
        e.cooldown = 99;
    }
    save(g, dir, "3-wave1-close");

    // Driving and turning past obstacles.
    Input drive = tilt;
    drive.forward = true;
    drive.accel[1] = -0.25f;
    run(g, drive, 2.5f);
    save(g, dir, "4-driving");

    // Stage all three models side by side at mid range, one winding up to fire.
    g.enemies.clear();
    for (int i = 0; i < 3; ++i) {
        Enemy e;
        e.type = static_cast<EnemyType>(i);
        float ang = g.heading + (i - 1) * 0.33f;
        e.x = g.px + std::sin(ang) * 70;
        e.z = g.pz + std::cos(ang) * 70;
        e.heading = wrapAngle(ang + PI + 0.4f);
        e.cooldown = 99;
        g.enemies.push_back(e);
    }
    g.enemies[1].tell = 5;
    g.world.obstacles.clear();
    g.time = 0.2f;
    save(g, dir, "5-three-models");

    // The Core line-up, same staging.
    {
        std::vector<Enemy> keep = g.enemies;
        for (int i = 0; i < 3; ++i) g.enemies[i].type = static_cast<EnemyType>(3 + i), g.enemies[i].tell = 0;
        save(g, dir, "5b-core-lineup");
        for (int i = 0; i < 3; ++i) g.enemies[i].heading += 0.9f;
        save(g, dir, "5c-core-turned");
        g.enemies = keep;
    }

    // Shoot the middle one: shell in flight, then the break-up.
    g.enemies[1].tell = 0;
    g.update(fire, STEP);
    run(g, fire, 0.2f);
    save(g, dir, "6-shot");
    run(g, none, 0.6f);
    save(g, dir, "7-breakup");

    // An enemy behind, then a hit, pause and game over.
    g.enemies[0].x = g.px - std::sin(g.heading) * 60;
    g.enemies[0].z = g.pz - std::cos(g.heading) * 60;
    Input esc;
    esc.pause = true;
    g.update(esc, STEP);
    g.update(none, STEP);
    save(g, dir, "8-paused");
    g.update(fire, STEP);
    g.update(none, STEP);
    g.shots.push_back({g.px, g.pz + 4, 0, -60, 2, false});
    run(g, none, 0.5f);
    save(g, dir, "9-hit");
    g.lives = 0;
    run(g, none, 2.0f);
    save(g, dir, "10-gameover");
    return 0;
}
