// Core tests. Plain checks, no framework: run by CTest, non-zero exit on failure.
#include <cstdio>

#include "game.h"

using namespace bz;

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                    \
        }                                                                  \
    } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((a) - (b)) < (eps))

static Frame frame;

static void testMaths() {
    NEAR(wrapAngle(3 * PI), PI, 1e-4f);
    NEAR(wrapAngle(-PI / 2 - 2 * PI), -PI / 2, 1e-4f);
    NEAR(bearingTo(0, 0, 0, 10), 0, 1e-5f);
    NEAR(bearingTo(0, 0, 10, 0), PI / 2, 1e-5f);
}

static void testRaster() {
    frame.clear();
    frame.line(-500, -500, 900, 900, WHITE); // clipped diagonal
    CHECK(frame.at(50, 50) == WHITE);
    CHECK(frame.at(0, 0) == WHITE);
    CHECK(frame.at(51, 50) == BLACK);
    frame.line(-10, 20, -5, 300, WHITE); // wholly outside: must not crash or draw
    frame.line(0, 100, 319, 100, AMBER);
    CHECK(frame.at(0, 100) == AMBER && frame.at(319, 100) == AMBER);
    frame.clear();
    int end = frame.text(10, 10, "AB", WHITE);
    CHECK(end == 22);
    CHECK(Frame::textWidth("AB") == 11);
    CHECK(frame.at(10, 16) == WHITE); // left stem of the A
}

static void testCamera() {
    Camera cam;
    cam.set(0, 0, 0, 3);
    float sx, sy;
    CHECK(projectPoint(cam, {0, 3, 50}, sx, sy));
    NEAR(sx, VIEW_CX, 0.01f);
    NEAR(sy, VIEW_CY, 0.01f);
    CHECK(projectPoint(cam, {10, 3, 50}, sx, sy) && sx > VIEW_CX); // right is right
    CHECK(projectPoint(cam, {0, 8, 50}, sx, sy) && sy < VIEW_CY);  // up is up
    CHECK(!projectPoint(cam, {0, 3, -50}, sx, sy));                // behind
    // Turned right 90 degrees, a point on +x is dead ahead.
    cam.set(0, 0, PI / 2, 3);
    CHECK(projectPoint(cam, {40, 3, 0}, sx, sy));
    NEAR(sx, VIEW_CX, 0.01f);
    // A segment crossing the near plane is clipped, not dropped.
    cam.set(0, 0, 0, 3);
    float o[4];
    CHECK(projectSegment(cam, {5, 3, -10}, {5, 3, 30}, o));
    CHECK(!projectSegment(cam, {5, 3, -10}, {5, 3, -30}, o));
}

static void feed(Tilt& t, float degrees, int n = 200) {
    float r = degrees * PI / 180;
    // Pitched 30 degrees towards the player as well: roll must read the same.
    float a[3] = {std::sin(r), std::cos(r) * 0.5f, std::cos(r) * 0.8660254f};
    for (int i = 0; i < n; ++i) t.update(a, 0, 1, STEP);
}

static void testTilt() {
    Tilt t;
    feed(t, 0);
    CHECK(t.steer() == 0);
    feed(t, 3);
    CHECK(t.steer() == 0); // inside the dead zone
    feed(t, 12);
    CHECK(t.steer() > 0.1f && t.steer() < 0.9f);
    feed(t, 30);
    NEAR(t.steer(), 1.0f, 1e-3f);
    feed(t, -30);
    NEAR(t.steer(), -1.0f, 1e-3f);
    // Recentre: whatever angle it is held at becomes "straight".
    feed(t, 15);
    t.recenter();
    CHECK(t.steer() == 0);
    feed(t, 40);
    NEAR(t.steer(), 1.0f, 1e-3f);
    // The filter smooths a sudden jump.
    Tilt s;
    feed(s, 0);
    feed(s, 30, 1);
    CHECK(s.steer() < 0.5f);
    // Sign flips the direction.
    Tilt inv;
    float a[3] = {0.5f, 0, 0.8f};
    for (int i = 0; i < 200; ++i) inv.update(a, 0, -1, STEP);
    CHECK(inv.steer() < 0);
}

static void testWorld() {
    Rng rng;
    World w;
    w.generate(rng);
    CHECK(w.obstacles.size() >= 20);
    CHECK(!w.blocked(0, 0, PLAYER_RADIUS)); // the start is clear
    const Obstacle& o = w.obstacles[0];
    CHECK(w.blocked(o.x, o.z, 1));
    CHECK(w.blocked(w.radius + 5, 0, 1)); // off the board
    Rng a, b;
    CHECK(a.next() == b.next()); // deterministic
}

static void testEnemies() {
    Rng rng;
    World empty;
    Target player = {0, 0, 0, 0};
    for (EnemyType type : {EnemyType::K132, EnemyType::V11, EnemyType::Adv}) {
        Enemy e;
        e.type = type;
        e.x = 0;
        e.z = 150;
        e.heading = PI;
        bool fired = false;
        for (int i = 0; i < 60 * 40 && !fired; ++i) fired = updateEnemy(e, empty, player, STEP, rng);
        CHECK(fired); // every model eventually closes in and shoots
        CHECK(dist2(e.x, e.z, 0, 0) < 130.0f * 130.0f);
        if (type != EnemyType::Adv) {
            float aim = wrapAngle(bearingTo(e.x, e.z, 0, 0) - e.heading);
            CHECK(std::fabs(aim) < 0.05f); // and is pointing at the player when it does
        }
        CHECK(e.cooldown > 1.0f);
    }
}

static Input held(bool fire = false, bool fwd = false) {
    Input in;
    in.fire = fire;
    in.forward = fwd;
    return in;
}

static void run(Game& g, const Input& in, float seconds) {
    for (int i = 0; i < static_cast<int>(seconds * 60); ++i) g.update(in, STEP);
}

static void testGame() {
    Game g(7);
    CHECK(g.state == State::Title);
    g.update(held(true), STEP);
    g.update(held(false), STEP);
    CHECK(g.state == State::Play); // no tilt sensor: straight into the game
    run(g, held(), 1.5f);
    CHECK(g.wave == 1 && g.aliveEnemies() == 1);

    // Put the enemy dead ahead and shoot it.
    g.world.obstacles.clear();
    g.enemies[0].x = 0;
    g.enemies[0].z = 60;
    g.enemies[0].cooldown = 99;
    g.px = g.pz = g.heading = 0;
    run(g, held(true), 1.0f);
    CHECK(g.aliveEnemies() == 0);
    CHECK(g.score == 1000);
    CHECK(!g.fragments.empty());
    run(g, held(), 3.0f);
    CHECK(g.wave == 2 && g.aliveEnemies() == 2);

    // Driving forward moves along the heading; a wall of obstacle stops it.
    for (Enemy& e : g.enemies) e.cooldown = 99, e.x = 200, e.z = -200;
    g.px = g.pz = g.heading = 0;
    run(g, held(false, true), 1.0f);
    CHECK(g.pz > 10 && std::fabs(g.px) < 0.01f);
    g.world.obstacles.push_back({ModelId::Brick, 0, g.pz + 14, 0, 9});
    float before = g.pz;
    run(g, held(false, true), 1.0f);
    CHECK(g.pz - before < 3.0f);
    g.world.obstacles.clear();

    // Steering by key turns right; tilt adds in when a sensor is present.
    Input right;
    right.right = true;
    g.heading = 0;
    run(g, right, 0.5f);
    CHECK(g.heading > 0.5f);
    Input tilted;
    tilted.tiltAvailable = true;
    tilted.accel[1] = -0.5f; // default axis y, sign -1: negative y means tilted right
    tilted.accel[2] = 0.8f;
    g.heading = 0;
    g.tilt = Tilt();
    for (int i = 0; i < 5; ++i) g.update(Input(), STEP);
    Input level;
    level.tiltAvailable = true;
    run(g, level, 0.2f);
    g.heading = 0;
    run(g, tilted, 0.6f);
    CHECK(g.heading > 0.3f);

    // Three hits end the game and record the high score.
    int scoreBefore = g.score;
    for (int life = 3; life > 0; --life) {
        CHECK(g.lives == life);
        g.shots.push_back({g.px, g.pz + 5, 0, -60, 2, false});
        run(g, held(), 0.3f);
        CHECK(g.state == State::Dying);
        run(g, held(), 2.0f);
        for (Enemy& e : g.enemies) e.cooldown = 99;
    }
    CHECK(g.state == State::GameOver);
    CHECK(g.highScore == scoreBefore && g.highScoreDirty);

    // Pause and quit.
    Game q(3);
    q.update(held(true), STEP);
    q.update(held(), STEP);
    Input esc;
    esc.pause = true;
    q.update(esc, STEP);
    q.update(held(), STEP);
    CHECK(q.state == State::Paused);
    q.update(esc, STEP);
    CHECK(q.wantsQuit);

    // Rendering every state stays inside the frame.
    g.render(frame);
    q.render(frame);
    Game t(5);
    t.render(frame);
    bool lit = false;
    for (Color c : frame.px) lit |= c != BLACK;
    CHECK(lit);
}

static void testCalibrate() {
    Game g(2);
    Input in;
    in.tiltAvailable = true;
    in.fire = true;
    g.update(in, STEP);
    in.fire = false;
    g.update(in, STEP);
    CHECK(g.state == State::Calibrate); // with a sensor, level is set first
    int sign = g.tiltSign;
    in.invert = true;
    g.update(in, STEP);
    in.invert = false;
    g.update(in, STEP);
    CHECK(g.tiltSign == -sign && g.tiltConfigDirty);
    g.render(frame);
    in.fire = true;
    g.update(in, STEP);
    CHECK(g.state == State::Play);
}

static void testSound() {
    static int16_t buf[SOUND_RATE * 2];
    Synth s;
    CHECK(s.silent());
    s.render(buf, 1000);
    bool quiet = true;
    for (int i = 0; i < 1000; ++i) quiet &= buf[i] == 0;
    CHECK(quiet); // nothing playing: digital silence

    s.trigger(SND_FIRE);
    CHECK(!s.silent());
    s.render(buf, SOUND_RATE / 10);
    int peak = 0;
    for (int i = 0; i < SOUND_RATE / 10; ++i) peak = std::abs(buf[i]) > peak ? std::abs(buf[i]) : peak;
    CHECK(peak > 3000);
    s.render(buf, SOUND_RATE); // the shot is 0.18 s long: finished by now
    CHECK(s.silent());

    // Everything at once stays inside 16 bits, and a flood of triggers is safe.
    for (int i = 0; i < 40; ++i) s.trigger(0xFF);
    s.setEngine(1);
    s.render(buf, SOUND_RATE * 2);
    for (int i = 0; i < SOUND_RATE * 2; ++i) peak = std::abs(buf[i]) > peak ? std::abs(buf[i]) : peak;
    CHECK(peak <= 32000);

    // Mute silences the output; the engine fades out when switched off.
    s.muted = true;
    s.render(buf, 500);
    CHECK(buf[250] == 0);
    s.muted = false;
    s.setEngine(0);
    s.render(buf, SOUND_RATE * 2);
    s.render(buf, SOUND_RATE * 2);
    CHECK(s.silent());

    // The game raises the right events.
    Game g(7);
    g.update(held(true), STEP);
    g.update(held(false), STEP);
    CHECK(g.engine == 0 || g.state == State::Play);
    run(g, held(), 1.5f);
    CHECK(g.sounds & SND_WAVE);
    CHECK(g.engine > 0.2f && g.engine < 0.3f); // idling
    g.sounds = 0;
    g.world.obstacles.clear();
    g.enemies[0].x = 0, g.enemies[0].z = 60, g.enemies[0].cooldown = 99;
    g.px = g.pz = g.heading = 0;
    run(g, held(true), 1.0f);
    CHECK((g.sounds & SND_FIRE) && (g.sounds & SND_KILL));
    run(g, held(false, true), 1.5f);
    CHECK(g.engine > 0.9f); // flat out
    Input m;
    m.mute = true;
    g.update(m, STEP);
    CHECK(g.muted);
    Input esc;
    esc.pause = true;
    g.update(esc, STEP);
    g.update(Input(), STEP);
    CHECK(g.state == State::Paused && g.engine == 0); // engine stops when paused
}

// What the CardputerZero's sensor reads for a given pose. Screen frame: x right, y top,
// z out of the screen. The sensor is mounted so that raw = (-screen.y, screen.x, screen.z)
// (M5Stack Compass app, screenToBmi270).
static void devicePose(float pitchDeg, float rollRightDeg, float raw[3]) {
    float p = pitchDeg * PI / 180, r = rollRightDeg * PI / 180;
    float sx = -std::sin(r), sy = std::sin(p) * std::cos(r), sz = std::cos(p) * std::cos(r);
    raw[0] = -sy;
    raw[1] = sx;
    raw[2] = sz;
}

static void testDeviceAxes() {
    // With the built-in defaults and no calibration changes, on the real mounting:
    for (float pitch : {0.0f, 40.0f, 80.0f}) { // flat on a table up to nearly upright
        Game g(4);
        Input in;
        in.tiltAvailable = true;
        devicePose(pitch, 0, in.accel);
        in.fire = true;
        g.update(in, STEP);
        in.fire = false;
        run(g, in, 0.3f);
        CHECK(g.state == State::Calibrate);
        in.fire = true;
        g.update(in, STEP); // set level, start
        in.fire = false;
        run(g, in, 0.5f);
        CHECK(g.state == State::Play);
        NEAR(g.heading, 0, 1e-3f); // held level: straight on

        devicePose(pitch, 20, in.accel); // right edge down
        run(g, in, 0.6f);
        CHECK(g.heading > 0.3f); // turns right

        g.heading = 0;
        devicePose(pitch, -20, in.accel); // left edge down
        run(g, in, 0.8f);
        CHECK(g.heading < -0.3f); // turns left

        // Tipping it towards or away from you must not steer.
        devicePose(pitch, 0, in.accel);
        run(g, in, 0.5f);
        g.heading = 0;
        devicePose(pitch + 8, 0, in.accel);
        run(g, in, 0.5f);
        NEAR(g.heading, 0, 1e-3f);
    }
}

int main() {
    testDeviceAxes();
    testSound();
    testMaths();
    testRaster();
    testCamera();
    testTilt();
    testWorld();
    testEnemies();
    testGame();
    testCalibrate();
    if (failures) {
        std::printf("%d check(s) failed\n", failures);
        return 1;
    }
    std::printf("all core tests passed\n");
    return 0;
}
