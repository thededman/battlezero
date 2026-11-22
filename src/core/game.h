// Game state and rules. No OS calls: a front end feeds Input and shows the Frame.
#pragma once
#include <vector>

#include "camera.h"
#include "enemy.h"
#include "input.h"
#include "raster.h"
#include "tilt.h"
#include "world.h"

namespace bz {

enum class State { Title, Calibrate, Play, Paused, Dying, GameOver };

struct Shot {
    float x, z, vx, vz, life;
    bool fromPlayer;
};

struct Fragment {
    Vec3 a, b, vel;
    float life;
    Color color;
};

struct SkyFeature {
    float bearing;
    int kind, size;
};

constexpr float PLAYER_SPEED = 20.0f;
constexpr float PLAYER_TURN = 1.5f; // radians per second at full steer
constexpr float PLAYER_RADIUS = 3.0f;
constexpr float PLAYER_EYE = 3.0f;
constexpr float SHOT_SPEED = 150.0f;
constexpr float SHOT_HEIGHT = 2.6f;
constexpr float STEP = 1.0f / 60.0f;

struct Game {
    explicit Game(uint32_t seed = 1);

    void update(const Input& in, float dt); // dt in seconds since the last call
    void render(Frame& f) const;

    void startGame();
    void spawnWave(int n);
    void spawnEnemy(EnemyType type);
    void killEnemy(Enemy& e);
    void playerHit();
    int aliveEnemies() const;

    // Things a front end loads, saves or acts on.
    int highScore = 0;
    bool highScoreDirty = false;
    int tiltAxis = 0, tiltSign = -1;
    bool tiltConfigDirty = false;
    bool wantsQuit = false;

    State state = State::Title;
    Rng rng;
    World world;
    Tilt tilt;
    bool tiltEnabled = true, tiltSeen = false;
    float time = 0;

    float px = 0, pz = 0, heading = 0, speed = 0, reload = 0;
    float pvx = 0, pvz = 0;
    bool atEdge = false;
    int score = 0, lives = 3, wave = 0;
    float waveDelay = 0, banner = 0, stateTime = 0, steer = 0;

    std::vector<Enemy> enemies;
    std::vector<Shot> shots;
    std::vector<Fragment> fragments;
    std::vector<SkyFeature> sky;
    float cracks[12][4] = {};

private:
    void step(const Input& in);
    void playStep(const Input& in);
    void renderPlay(Frame& f) const;
    void renderHud(Frame& f) const;

    struct Presses {
        bool fire = false, pause = false, recenter = false, tiltToggle = false, invert = false,
             axis = false;
    } pressed;
    Input prev;
    float acc = 0;
};

} // namespace bz
