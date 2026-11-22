#include "enemy.h"

namespace bz {

const EnemySpec& spec(EnemyType t) {
    static const EnemySpec specs[] = {
        // model, colour, speed, turn, shot speed, reload, tell, score, name, radius, hp, burst, gap
        {ModelId::CardK132, AMBER, 8.0f, 0.7f, 70.0f, 3.2f, 0.45f, 1000, "CARDPUTER", 6.5f, 1, 1, 0},
        {ModelId::CardV11, WHITE, 13.0f, 1.0f, 85.0f, 2.6f, 0.60f, 2000, "CARDPUTER V1.1", 6.5f, 1, 1, 0},
        {ModelId::CardAdv, VIOLET, 17.0f, 1.5f, 100.0f, 2.2f, 0.25f, 3000, "CARDPUTER-ADV", 6.5f, 1, 1, 0},
        {ModelId::CoreBasic, GREY, 7.0f, 0.8f, 75.0f, 3.6f, 0.50f, 2500, "CORE BASIC", 4.8f, 2, 3, 0.22f},
        {ModelId::CoreFire, ROSE, 19.0f, 1.6f, 95.0f, 1.8f, 0.20f, 3500, "CORE FIRE", 4.8f, 1, 2, 0.15f},
        {ModelId::Core2, GREEN, 11.0f, 1.2f, 170.0f, 4.0f, 1.00f, 4000, "CORE2", 4.8f, 1, 1, 0},
    };
    return specs[static_cast<int>(t)];
}

namespace {

const float AIM_TOLERANCE = 0.035f;
const float PLAYER_KEEP_OUT = 12.0f;

void turnToward(Enemy& e, float desired, float rate, float dt) {
    float err = wrapAngle(desired - e.heading);
    float step = rate * dt;
    e.heading = wrapAngle(e.heading + clampf(err, -step, step));
}

// Turns towards `desired` and drives forward, steering round anything in the way.
void drive(Enemy& e, const World& world, const Target& p, float desired, float factor, float dt) {
    const EnemySpec& s = spec(e.type);
    if (e.avoid > 0) {
        e.avoid -= dt;
        desired += 1.3f * static_cast<float>(e.side);
    }
    turnToward(e, desired, s.turnRate, dt);
    float nx = e.x + std::sin(e.heading) * s.speed * factor * dt;
    float nz = e.z + std::cos(e.heading) * s.speed * factor * dt;
    bool hit = world.blocked(nx, nz, ENEMY_RADIUS) ||
               dist2(nx, nz, p.x, p.z) < PLAYER_KEEP_OUT * PLAYER_KEEP_OUT;
    if (hit) {
        e.avoid = 0.8f;
        e.heading = wrapAngle(e.heading + s.turnRate * dt * static_cast<float>(e.side));
        return;
    }
    e.x = nx;
    e.z = nz;
}

// Holds still, lines up on `aim`, then winds up. Returns true once the wind-up starts.
bool aimAndWindUp(Enemy& e, float aim, float dt) {
    const EnemySpec& s = spec(e.type);
    turnToward(e, aim, s.turnRate * 1.5f, dt);
    if (e.cooldown <= 0 && std::fabs(wrapAngle(aim - e.heading)) < AIM_TOLERANCE) {
        e.tell = s.tellTime;
        e.windingUp = true;
        return true;
    }
    return false;
}

} // namespace

bool updateEnemy(Enemy& e, const World& world, const Target& p, float dt, Rng& rng) {
    const EnemySpec& s = spec(e.type);
    if (e.cooldown > 0) e.cooldown -= dt;
    if (e.flash > 0) e.flash -= dt;

    // Mid-burst: hold position, keep tracking, loose the remaining shots.
    if (e.burstLeft > 0) {
        turnToward(e, bearingTo(e.x, e.z, p.x, p.z), s.turnRate, dt);
        e.burstTimer -= dt;
        if (e.burstTimer <= 0) {
            e.burstLeft -= 1;
            e.burstTimer = s.burstGap;
            return true;
        }
        return false;
    }

    // Winding up: frozen until the shot goes.
    if (e.tell > 0) {
        e.tell -= dt;
        if (e.tell <= 0) {
            e.tell = 0;
            e.cooldown = s.reload;
            e.burstLeft = s.burst - 1;
            e.burstTimer = s.burstGap;
            e.state = 2;
            e.stateTime = 0;
            return true;
        }
        return false;
    }

    float bearing = bearingTo(e.x, e.z, p.x, p.z);
    float dist = std::sqrt(dist2(e.x, e.z, p.x, p.z));
    e.stateTime += dt;

    switch (e.type) {
    case EnemyType::K132:
    case EnemyType::CoreBasic:
        // Plods into range, stops and shoots. The Core Basic does the same with a
        // three-round burst (one per button) and shrugs off the first hit.
        if (dist > (e.type == EnemyType::K132 ? 95.0f : 85.0f)) drive(e, world, p, bearing, 1.0f, dt);
        else aimAndWindUp(e, bearing, dt);
        break;

    case EnemyType::V11: {
        // Circles the player, pausing to line up a shot.
        if (e.state == 1) {
            aimAndWindUp(e, bearing, dt);
            if (e.stateTime > 4.0f) e.state = 0, e.stateTime = 0;
        } else {
            if (e.state == 2) { // just fired: maybe reverse the circle
                if (rng.uniform() < 0.4f) e.side = -e.side;
                e.state = 0;
            }
            float offset = dist > 80.0f ? 0.9f : (dist < 60.0f ? 2.0f : 1.5f);
            drive(e, world, p, bearing + offset * static_cast<float>(e.side), 1.0f, dt);
            if (e.cooldown <= 0 && e.stateTime > 2.5f && dist < 120.0f) e.state = 1, e.stateTime = 0;
        }
        break;
    }

    case EnemyType::Adv: {
        // Weaves in, shoots where the player is heading, then dodges sideways.
        if (e.state == 2) {
            drive(e, world, p, bearing + 1.6f * static_cast<float>(e.side), 1.0f, dt);
            if (e.stateTime > 1.8f) {
                e.state = 0;
                e.stateTime = 0;
                e.side = -e.side;
            }
        } else if (e.state == 1) {
            float t = dist / s.shotSpeed;
            float aim = bearingTo(e.x, e.z, p.x + p.vx * t, p.z + p.vz * t);
            aimAndWindUp(e, aim, dt);
            if (e.stateTime > 3.0f) e.state = 0, e.stateTime = 0;
        } else {
            float weave = std::sin(e.stateTime * 2.6f) * 0.5f;
            drive(e, world, p, bearing + weave, dist > 60.0f ? 1.0f : 0.3f, dt);
            if (e.cooldown <= 0 && dist < 110.0f) e.state = 1, e.stateTime = 0;
        }
        break;
    }

    case EnemyType::CoreFire:
        // Charges in close, fires a quick pair, swings away and comes round again.
        if (e.state == 2) {
            drive(e, world, p, bearing + 1.4f * static_cast<float>(e.side), 1.0f, dt);
            if (e.stateTime > 1.3f) {
                e.state = 0;
                e.stateTime = 0;
                e.side = -e.side;
            }
        } else if (dist > 40.0f) {
            drive(e, world, p, bearing, 1.0f, dt);
        } else {
            aimAndWindUp(e, bearing, dt);
        }
        break;

    case EnemyType::Core2: {
        // Sniper: keeps its distance and takes a slow, fast-flying, well-aimed shot.
        if (dist < 110.0f) {
            drive(e, world, p, bearing + PI, 1.0f, dt);
        } else if (dist > 175.0f) {
            drive(e, world, p, bearing, 1.0f, dt);
        } else {
            float t = dist / s.shotSpeed;
            aimAndWindUp(e, bearingTo(e.x, e.z, p.x + p.vx * t, p.z + p.vz * t), dt);
        }
        break;
    }

    case EnemyType::COUNT:
        break;
    }
    return false;
}

} // namespace bz
