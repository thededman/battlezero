#include "game.h"

namespace bz {

Game::Game(uint32_t seed) {
    rng.s = seed;
    world.generate(rng);
    for (int i = 0; i < 18; ++i)
        sky.push_back({rng.range(-PI, PI), static_cast<int>(rng.next() % 2),
                       4 + static_cast<int>(rng.next() % 9)});
}

int Game::aliveEnemies() const {
    int n = 0;
    for (const Enemy& e : enemies) n += e.alive ? 1 : 0;
    return n;
}

void Game::startGame() {
    state = State::Play;
    px = pz = heading = speed = reload = 0;
    score = 0;
    lives = 3;
    wave = 0;
    waveDelay = 1.0f;
    banner = 0;
    enemies.clear();
    shots.clear();
    fragments.clear();
}

void Game::spawnEnemy(EnemyType type) {
    Enemy e;
    e.type = type;
    e.x = e.z = 0;
    for (int tries = 0; tries < 60; ++tries) {
        float ang = rng.range(-PI, PI), d = rng.range(150.0f, 200.0f);
        float x = px + std::sin(ang) * d, z = pz + std::cos(ang) * d;
        if (world.blocked(x, z, ENEMY_RADIUS + 2.0f)) continue;
        e.x = x;
        e.z = z;
        break;
    }
    e.heading = bearingTo(e.x, e.z, px, pz);
    e.hp = spec(type).hp;
    e.cooldown = rng.range(2.0f, 4.0f);
    e.side = rng.uniform() < 0.5f ? -1 : 1;
    enemies.push_back(e);
}

void Game::spawnWave(int n) {
    wave = n;
    banner = 2.0f;
    sounds |= SND_WAVE;
    enemies.clear();
    using T = EnemyType;
    switch (n) {
    case 1: spawnEnemy(T::K132); break;
    case 2: spawnEnemy(T::K132), spawnEnemy(T::K132); break;
    case 3: spawnEnemy(T::V11); break;
    case 4: spawnEnemy(T::V11), spawnEnemy(T::K132); break;
    case 5: spawnEnemy(T::Adv); break;
    case 6: spawnEnemy(T::CoreBasic); break;
    case 7: spawnEnemy(T::Adv), spawnEnemy(T::V11); break;
    case 8: spawnEnemy(T::CoreFire); break;
    case 9: spawnEnemy(T::CoreBasic), spawnEnemy(T::V11); break;
    case 10: spawnEnemy(T::Core2); break;
    case 11: spawnEnemy(T::CoreFire), spawnEnemy(T::Adv); break;
    case 12: spawnEnemy(T::Core2), spawnEnemy(T::CoreBasic); break;
    default:
        for (int i = 0; i < 3; ++i) spawnEnemy(static_cast<T>(rng.next() % static_cast<uint32_t>(T::COUNT)));
    }
}

// A shell has struck: armoured models survive the first one.
void Game::hitEnemy(Enemy& e) {
    e.hp -= 1;
    if (e.hp <= 0) {
        killEnemy(e);
        return;
    }
    e.flash = 0.25f;
    sounds |= SND_BUMP;
}

void Game::killEnemy(Enemy& e) {
    e.alive = false;
    sounds |= SND_KILL;
    const EnemySpec& s = spec(e.type);
    score += s.score;
    const Model& m = model(s.model);
    float sn = std::sin(e.heading), cs = std::cos(e.heading);
    for (int i = 0; i < m.coarseEdges; ++i) {
        Vec3 a = m.v[m.e[i].first], b = m.v[m.e[i].second];
        Vec3 wa = {e.x + a.x * cs + a.z * sn, a.y, e.z - a.x * sn + a.z * cs};
        Vec3 wb = {e.x + b.x * cs + b.z * sn, b.y, e.z - b.x * sn + b.z * cs};
        float mx = (wa.x + wb.x) / 2 - e.x, mz = (wa.z + wb.z) / 2 - e.z;
        Vec3 vel = {mx * 1.5f + rng.range(-6, 6), rng.range(6, 16), mz * 1.5f + rng.range(-6, 6)};
        fragments.push_back({wa, wb, vel, rng.range(1.0f, 1.8f), s.color});
    }
}

void Game::playerHit() {
    lives -= 1;
    sounds |= SND_HIT;
    state = State::Dying;
    stateTime = 0;
    speed = 0;
    shots.clear();
    for (auto& c : cracks) {
        float ang = rng.range(-PI, PI), r0 = rng.range(0, 25), r1 = rng.range(60, 200);
        float ox = VIEW_CX + rng.range(-30, 30), oy = SCREEN_H / 2.0f + rng.range(-20, 20);
        c[0] = ox + std::cos(ang) * r0;
        c[1] = oy + std::sin(ang) * r0;
        c[2] = ox + std::cos(ang) * r1;
        c[3] = oy + std::sin(ang) * r1;
    }
}

void Game::update(const Input& in, float dt) {
    pressed.fire |= in.fire && !prev.fire;
    pressed.pause |= in.pause && !prev.pause;
    pressed.recenter |= in.recenter && !prev.recenter;
    pressed.tiltToggle |= in.tiltToggle && !prev.tiltToggle;
    pressed.invert |= in.invert && !prev.invert;
    pressed.axis |= in.axis && !prev.axis;
    pressed.mute |= in.mute && !prev.mute;
    prev = in;

    acc += clampf(dt, 0.0f, 0.1f);
    while (acc >= STEP) {
        step(in);
        pressed = Presses();
        acc -= STEP;
    }
}

void Game::step(const Input& in) {
    time += STEP;
    stateTime += STEP;
    tiltSeen = in.tiltAvailable;
    if (in.tiltAvailable) tilt.update(in.accel, tiltAxis, tiltSign, STEP);
    if (pressed.recenter) tilt.recenter();
    if (pressed.mute) muted = !muted;
    engine = 0;

    switch (state) {
    case State::Title:
        if (pressed.pause) wantsQuit = true;
        if (pressed.fire) {
            if (in.tiltAvailable) state = State::Calibrate, stateTime = 0;
            else startGame();
        }
        break;

    case State::Calibrate:
        if (pressed.invert) tiltSign = -tiltSign, tiltConfigDirty = true;
        if (pressed.axis) tiltAxis = (tiltAxis + 1) % 3, tiltConfigDirty = true, tilt.primed = false;
        if (pressed.pause) state = State::Title;
        if (pressed.fire) {
            tilt.recenter();
            tiltEnabled = true;
            startGame();
        }
        break;

    case State::Play:
        playStep(in);
        break;

    case State::Paused:
        if (pressed.fire) state = State::Play;
        else if (pressed.pause) wantsQuit = true;
        break;

    case State::Dying:
        for (Fragment& f : fragments) f.life -= STEP;
        if (stateTime > 1.6f) {
            if (lives <= 0) {
                state = State::GameOver;
                sounds |= SND_GAME_OVER;
                stateTime = 0;
                if (score > highScore) highScore = score, highScoreDirty = true;
            } else {
                // Push the survivors back so the player gets a fair restart.
                std::vector<Enemy> old;
                old.swap(enemies);
                for (const Enemy& e : old)
                    if (e.alive) spawnEnemy(e.type);
                state = State::Play;
            }
        }
        break;

    case State::GameOver:
        if (stateTime > 1.0f && (pressed.fire || pressed.pause)) state = State::Title;
        break;
    }
}

void Game::playStep(const Input& in) {
    if (pressed.pause) {
        state = State::Paused;
        return;
    }
    if (pressed.tiltToggle) tiltEnabled = !tiltEnabled;

    // Steering: keys always work; tilt adds to them when it is on.
    steer = (in.right ? 1.0f : 0.0f) - (in.left ? 1.0f : 0.0f);
    if (tiltEnabled && in.tiltAvailable) steer += tilt.steer();
    steer = clampf(steer, -1.0f, 1.0f);
    heading = wrapAngle(heading + steer * PLAYER_TURN * STEP);

    float target = in.forward ? PLAYER_SPEED : (in.reverse ? -PLAYER_SPEED * 0.6f : 0.0f);
    speed += clampf(target - speed, -60.0f * STEP, 60.0f * STEP);

    float fx = std::sin(heading), fz = std::cos(heading);
    float nx = px + fx * speed * STEP, nz = pz + fz * speed * STEP;
    bool bump = world.hitsObstacle(nx, nz, PLAYER_RADIUS);
    for (const Enemy& e : enemies)
        if (e.alive && dist2(nx, nz, e.x, e.z) < (ENEMY_RADIUS + PLAYER_RADIUS) * (ENEMY_RADIUS + PLAYER_RADIUS))
            bump = true;
    atEdge = !world.inside(nx, nz, PLAYER_RADIUS);
    if (bump || atEdge) {
        if (std::fabs(speed) > 4.0f) sounds |= SND_BUMP;
        speed = 0;
        pvx = pvz = 0;
    } else {
        pvx = fx * speed;
        pvz = fz * speed;
        px = nx;
        pz = nz;
    }

    // One shell in flight at a time.
    reload -= STEP;
    bool mine = false;
    for (const Shot& s : shots) mine |= s.fromPlayer;
    if (in.fire && reload <= 0 && !mine) {
        shots.push_back({px + fx * 3.5f, pz + fz * 3.5f, fx * SHOT_SPEED, fz * SHOT_SPEED, 2.2f, true});
        reload = 0.5f;
        sounds |= SND_FIRE;
    }

    Target me = {px, pz, pvx, pvz};
    for (Enemy& e : enemies) {
        if (!e.alive) continue;
        bool fired = updateEnemy(e, world, me, STEP, rng);
        if (e.windingUp) sounds |= SND_TELL, e.windingUp = false;
        if (fired) {
            sounds |= SND_ENEMY_FIRE;
            const EnemySpec& s = spec(e.type);
            float ex = std::sin(e.heading), ez = std::cos(e.heading);
            shots.push_back({e.x + ex * 3.0f, e.z + ez * 3.0f, ex * s.shotSpeed, ez * s.shotSpeed, 4.0f, false});
        }
    }

    bool hit = false;
    for (size_t i = 0; i < shots.size();) {
        Shot& s = shots[i];
        s.x += s.vx * STEP;
        s.z += s.vz * STEP;
        s.life -= STEP;
        bool dead = s.life <= 0 || world.blocked(s.x, s.z, 0.3f);
        if (!dead && s.fromPlayer) {
            for (Enemy& e : enemies)
                if (e.alive && dist2(s.x, s.z, e.x, e.z) < spec(e.type).radius * spec(e.type).radius) {
                    hitEnemy(e);
                    dead = true;
                    break;
                }
        } else if (!dead && dist2(s.x, s.z, px, pz) < (PLAYER_RADIUS + 0.5f) * (PLAYER_RADIUS + 0.5f)) {
            hit = true;
            dead = true;
        }
        if (dead) shots.erase(shots.begin() + static_cast<long>(i));
        else ++i;
    }

    for (size_t i = 0; i < fragments.size();) {
        Fragment& f = fragments[i];
        f.life -= STEP;
        f.vel.y -= 20.0f * STEP;
        Vec3 d = {f.vel.x * STEP, f.vel.y * STEP, f.vel.z * STEP};
        f.a = {f.a.x + d.x, f.a.y + d.y, f.a.z + d.z};
        f.b = {f.b.x + d.x, f.b.y + d.y, f.b.z + d.z};
        if (f.life <= 0) fragments.erase(fragments.begin() + static_cast<long>(i));
        else ++i;
    }

    if (banner > 0) banner -= STEP;
    engine = 0.25f + 0.75f * std::fabs(speed) / PLAYER_SPEED;
    if (hit) {
        playerHit();
        return;
    }
    if (aliveEnemies() == 0) {
        waveDelay -= STEP;
        if (waveDelay <= 0) {
            spawnWave(wave + 1);
            waveDelay = 2.0f;
        }
    }
}

} // namespace bz
