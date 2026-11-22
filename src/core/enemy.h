// The older Cardputers and how each one fights.
#pragma once
#include "raster.h"
#include "world.h"

namespace bz {

enum class EnemyType { K132, V11, Adv };

struct EnemySpec {
    ModelId model;
    Color color;
    float speed, turnRate, shotSpeed, reload, tellTime;
    int score;
    const char* name;
};

const EnemySpec& spec(EnemyType t);

struct Enemy {
    EnemyType type = EnemyType::K132;
    float x = 0, z = 0, heading = 0;
    float cooldown = 2;  // seconds until it may fire again
    float tell = 0;      // > 0 while winding up a shot; it holds still
    int state = 0;
    float stateTime = 0;
    int side = 1;        // which way it circles or dodges
    float avoid = 0;     // > 0 while steering round something
    bool alive = true;
    bool windingUp = false; // set on the step a wind-up begins, for the warning blip
};

struct Target {
    float x, z, vx, vz;
};

constexpr float ENEMY_RADIUS = 6.5f;

// Advances one enemy. Returns true on the step it fires (along its heading).
bool updateEnemy(Enemy& e, const World& world, const Target& player, float dt, Rng& rng);

} // namespace bz
