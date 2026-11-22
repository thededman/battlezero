// Everything the player sees.
#include <cstdio>

#include "game.h"

namespace bz {
namespace {

void drawModel(Frame& f, const Camera& cam, const Model& m, float x, float z, float heading,
               Color color, bool fine) {
    float sn = std::sin(heading), cs = std::cos(heading);
    int n = fine ? static_cast<int>(m.e.size()) : m.coarseEdges;
    float o[4];
    for (int i = 0; i < n; ++i) {
        Vec3 a = m.v[m.e[i].first], b = m.v[m.e[i].second];
        Vec3 wa = {x + a.x * cs + a.z * sn, a.y, z - a.x * sn + a.z * cs};
        Vec3 wb = {x + b.x * cs + b.z * sn, b.y, z - b.x * sn + b.z * cs};
        if (projectSegment(cam, wa, wb, o)) f.line(o[0], o[1], o[2], o[3], color);
    }
}

void seg(Frame& f, const Camera& cam, Vec3 a, Vec3 b, Color c) {
    float o[4];
    if (projectSegment(cam, a, b, o)) f.line(o[0], o[1], o[2], o[3], c);
}

void tiltBar(Frame& f, const Tilt& tilt, int cx, int y) {
    const int half = 60;
    f.rect(cx - half, y, half * 2 + 1, 7, CYAN_DIM);
    f.line(static_cast<float>(cx), static_cast<float>(y - 2), static_cast<float>(cx),
           static_cast<float>(y + 8), CYAN_DIM);
    float t = clampf(tilt.angle() / tilt.fullAngle, -1.0f, 1.0f);
    int x = cx + static_cast<int>(t * (half - 3));
    f.fillRect(x - 2, y + 1, 5, 5, tilt.steer() != 0 ? CYAN : CYAN_DIM);
}

} // namespace

void Game::render(Frame& f) const {
    f.clear();
    char buf[48];

    if (state == State::Title) {
        Camera cam;
        cam.set(0, 0, 0, 4.0f);
        const ModelId spin[] = {ModelId::CardK132, ModelId::CardAdv};
        int which = static_cast<int>(time / 8.0f) % 2;
        drawModel(f, cam, model(spin[which]), 0, 30, time * 0.8f + PI, AMBER, true);
    }

    switch (state) {
    case State::Title:
        f.textCentered(160, 8, "BATTLEZERO", CYAN, 3);
        if (static_cast<int>(time * 2) % 2 == 0) f.textCentered(160, 142, "PRESS SPACE TO START", WHITE);
        std::snprintf(buf, sizeof buf, "HIGH SCORE %06d", highScore);
        f.textCentered(160, 156, buf, AMBER_DIM);
        return;

    case State::Calibrate:
        f.textCentered(160, 10, "TILT TO STEER", CYAN, 2);
        f.textCentered(160, 40, "HOLD IT HOW YOU WILL PLAY", WHITE);
        f.textCentered(160, 52, "TILT RIGHT: THE BLOCK SHOULD GO RIGHT", AMBER);
        tiltBar(f, tilt, 160, 76);
        f.textCentered(160, 98, "I: FLIP DIRECTION   A: OTHER AXIS", AMBER_DIM);
        std::snprintf(buf, sizeof buf, "AXIS %c  DIRECTION %c", "XYZ"[tiltAxis], tiltSign > 0 ? '+' : '-');
        f.textCentered(160, 110, buf, AMBER_DIM);
        f.textCentered(160, 134, "SPACE: SET LEVEL AND START", WHITE);
        f.textCentered(160, 150, "F/X DRIVE  SPACE FIRE  R RECENTRE", CYAN_DIM);
        return;

    default:
        break;
    }

    renderPlay(f);
    renderHud(f);

    if (state == State::Paused) {
        f.fillRect(90, 52, 140, 62, BLACK);
        f.rect(90, 52, 140, 62, CYAN);
        f.textCentered(160, 60, "PAUSED", CYAN, 2);
        f.textCentered(160, 84, "SPACE: RESUME", WHITE);
        f.textCentered(160, 98, "ESC: QUIT", WHITE);
    } else if (state == State::Dying) {
        int n = static_cast<int>(stateTime * 30);
        for (int i = 0; i < 12 && i < n; ++i)
            f.line(cracks[i][0], cracks[i][1], cracks[i][2], cracks[i][3], WHITE);
    } else if (state == State::GameOver) {
        f.textCentered(160, 56, "GAME OVER", RED, 3);
        std::snprintf(buf, sizeof buf, "SCORE %06d", score);
        f.textCentered(160, 90, buf, WHITE);
        if (score >= highScore && score > 0) f.textCentered(160, 104, "NEW HIGH SCORE", CYAN);
    } else if (banner > 0) {
        std::snprintf(buf, sizeof buf, "WAVE %d", wave);
        f.textCentered(160, 34, buf, CYAN, 2);
    }
}

void Game::renderPlay(Frame& f) const {
    Camera cam;
    cam.set(px, pz, heading, PLAYER_EYE);

    // Far skyline: pin headers and antenna masts, positioned by bearing only.
    for (const SkyFeature& s : sky) {
        float rel = wrapAngle(s.bearing - heading);
        if (std::fabs(rel) > 0.7f) continue;
        float x = VIEW_CX + FOCAL * std::tan(rel);
        float y = VIEW_CY;
        if (s.kind == 0) {
            float h = static_cast<float>(s.size + 6);
            f.line(x, y, x, y - h, AMBER_DARK);
            f.line(x - 3, y - h + 3, x + 3, y - h + 3, AMBER_DARK);
            f.line(x - 2, y - h + 6, x + 2, y - h + 6, AMBER_DARK);
        } else {
            for (int i = 0; i < s.size; ++i)
                f.line(x + i * 3.0f, y, x + i * 3.0f, y - 4, AMBER_DARK);
            f.line(x - 1, y - 1, x + s.size * 3.0f - 2, y - 1, AMBER_DARK);
        }
    }
    f.line(0, VIEW_CY, SCREEN_W - 1, VIEW_CY, AMBER_DIM);

    // Ground dots on a world grid, so movement and turning read clearly.
    const float G = 20.0f, R = 130.0f;
    float gx0 = std::floor((px - R) / G) * G, gz0 = std::floor((pz - R) / G) * G;
    for (float gx = gx0; gx <= px + R; gx += G)
        for (float gz = gz0; gz <= pz + R; gz += G) {
            float sx, sy;
            if (dist2(gx, gz, px, pz) < R * R && projectPoint(cam, {gx, 0, gz}, sx, sy))
                f.pixel(static_cast<int>(sx), static_cast<int>(sy), AMBER_DIM);
        }

    for (const Obstacle& o : world.obstacles) {
        float d2 = dist2(o.x, o.z, px, pz);
        if (d2 > 280.0f * 280.0f) continue;
        drawModel(f, cam, model(o.id), o.x, o.z, o.heading, AMBER_DIM, d2 < 80.0f * 80.0f);
    }

    for (const Enemy& e : enemies) {
        if (!e.alive) continue;
        const EnemySpec& s = spec(e.type);
        float d2 = dist2(e.x, e.z, px, pz);
        drawModel(f, cam, model(s.model), e.x, e.z, e.heading, s.color, d2 < 90.0f * 90.0f);
        // Wind-up tell: the infrared emitter on the top edge flashes before a shot.
        if (e.tell > 0 && static_cast<int>(time * 16) % 2 == 0) {
            Vec3 t = {e.x, CARD_H + 1.6f, e.z};
            seg(f, cam, {t.x, t.y - 1.0f, t.z}, {t.x, t.y + 1.0f, t.z}, RED);
            float rs = std::cos(heading), rc = -std::sin(heading);
            seg(f, cam, {t.x - rs, t.y, t.z - rc}, {t.x + rs, t.y, t.z + rc}, RED);
        }
    }

    for (const Shot& s : shots) {
        Color c = s.fromPlayer ? CYAN : RED;
        float len = std::sqrt(s.vx * s.vx + s.vz * s.vz);
        float ux = s.vx / len, uz = s.vz / len;
        seg(f, cam, {s.x - ux * 1.5f, SHOT_HEIGHT, s.z - uz * 1.5f}, {s.x + ux * 1.5f, SHOT_HEIGHT, s.z + uz * 1.5f}, c);
        seg(f, cam, {s.x, SHOT_HEIGHT - 0.5f, s.z}, {s.x, SHOT_HEIGHT + 0.5f, s.z}, c);
        seg(f, cam, {s.x - uz * 0.5f, SHOT_HEIGHT, s.z + ux * 0.5f}, {s.x + uz * 0.5f, SHOT_HEIGHT, s.z - ux * 0.5f}, c);
    }

    for (const Fragment& fr : fragments)
        if (fr.a.y > 0 || fr.b.y > 0) seg(f, cam, fr.a, fr.b, fr.life > 0.4f ? fr.color : AMBER_DARK);
}

void Game::renderHud(Frame& f) const {
    char buf[32];
    f.fillRect(0, 0, SCREEN_W, 13, BLACK);
    f.line(0, 13, SCREEN_W - 1, 13, CYAN_DIM);

    std::snprintf(buf, sizeof buf, "%06d", score);
    f.text(3, 3, buf, CYAN);
    std::snprintf(buf, sizeof buf, "W%d", wave);
    f.text(46, 3, buf, CYAN_DIM);

    // Lives as battery cells.
    for (int i = 0; i < 3; ++i) {
        int x = SCREEN_W - 14 - i * 14;
        f.rect(x, 3, 10, 7, i < lives ? CYAN : CYAN_DIM);
        f.fillRect(x + 10, 5, 1, 3, i < lives ? CYAN : CYAN_DIM);
        if (i < lives) f.fillRect(x + 2, 5, 6, 3, CYAN);
    }

    // Heading strip: 90 degrees either side of where you are facing.
    const int sx0 = 100, sx1 = 220, mid = 160;
    f.line(static_cast<float>(sx0), 10, static_cast<float>(sx1), 10, CYAN_DIM);
    f.line(static_cast<float>(mid), 1, static_cast<float>(mid), 4, CYAN);
    const char* names[] = {"N", "E", "S", "W"};
    for (int i = 0; i < 12; ++i) {
        float rel = wrapAngle(i * PI / 6 - heading);
        if (std::fabs(rel) > PI / 2) continue;
        int x = mid + static_cast<int>(rel / (PI / 2) * 60);
        if (i % 3 == 0) {
            if (std::abs(x - mid) < 56) f.text(x - 2, 2, names[i / 3], CYAN_DIM);
        } else {
            f.line(static_cast<float>(x), 8, static_cast<float>(x), 10, CYAN_DIM);
        }
    }

    bool locked = false;
    const float halfFov = std::atan(VIEW_CX / FOCAL);
    for (const Enemy& e : enemies) {
        if (!e.alive) continue;
        float rel = wrapAngle(bearingTo(px, pz, e.x, e.z) - heading);
        float dist = std::sqrt(dist2(px, pz, e.x, e.z));
        Color c = spec(e.type).color;
        if (std::fabs(rel) <= PI / 2) {
            int x = mid + static_cast<int>(rel / (PI / 2) * 60);
            f.fillRect(x - 1, 9, 3, 3, c);
        }
        if (std::fabs(rel) < std::atan(ENEMY_RADIUS / dist)) locked = true;
        // Off-screen threat arrows on the left or right edge.
        if (std::fabs(rel) > halfFov && state == State::Play) {
            bool right = rel > 0;
            float tip = right ? SCREEN_W - 3.0f : 2.0f, back = right ? SCREEN_W - 10.0f : 9.0f;
            float y = VIEW_CY + (std::fabs(rel) > PI / 2 ? 14.0f : 0.0f);
            f.line(back, y - 5, tip, y, c);
            f.line(back, y + 5, tip, y, c);
            f.line(back, y - 5, back, y + 5, c);
        }
    }

    // Reticle.
    Color rc = locked ? WHITE : CYAN_DIM;
    float cx = VIEW_CX, cy = VIEW_CY - 2;
    f.line(cx - 12, cy, cx - 4, cy, rc);
    f.line(cx + 4, cy, cx + 12, cy, rc);
    f.line(cx, cy - 9, cx, cy - 4, rc);
    f.line(cx, cy + 4, cx, cy + 9, rc);
    if (locked) f.rect(static_cast<int>(cx) - 3, static_cast<int>(cy) - 3, 7, 7, WHITE);

    if (atEdge && state == State::Play) f.textCentered(160, 150, "EDGE OF THE BOARD", RED);
    if (tiltSeen && !tiltEnabled) f.text(3, 160, "TILT OFF", CYAN_DIM);
    else if (tiltSeen) {
        // Small tilt needle bottom-left so the player can see their centre.
        f.line(8, 164, 40, 164, CYAN_DIM);
        int x = 24 + static_cast<int>(clampf(tilt.angle() / tilt.fullAngle, -1.0f, 1.0f) * 16);
        f.fillRect(x - 1, 162, 3, 5, CYAN);
    }
}

} // namespace bz
