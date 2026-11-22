#include "models.h"

namespace bz {
namespace {

uint16_t addV(Model& m, float x, float y, float z) {
    m.v.push_back({x, y, z});
    return static_cast<uint16_t>(m.v.size() - 1);
}

void addLine(Model& m, Vec3 a, Vec3 b) {
    uint16_t i = addV(m, a.x, a.y, a.z), j = addV(m, b.x, b.y, b.z);
    m.e.push_back({i, j});
}

// Box centred on (cx, cz) with its base at y0.
void addBox(Model& m, float cx, float y0, float cz, float w, float h, float d) {
    float x0 = cx - w / 2, x1 = cx + w / 2, z0 = cz - d / 2, z1 = cz + d / 2, y1 = y0 + h;
    uint16_t b = static_cast<uint16_t>(m.v.size());
    addV(m, x0, y0, z0); addV(m, x1, y0, z0); addV(m, x1, y1, z0); addV(m, x0, y1, z0);
    addV(m, x0, y0, z1); addV(m, x1, y0, z1); addV(m, x1, y1, z1); addV(m, x0, y1, z1);
    const int idx[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (auto& p : idx)
        m.e.push_back({static_cast<uint16_t>(b + p[0]), static_cast<uint16_t>(b + p[1])});
}

// Rectangle on the plane z = zf.
void addRect(Model& m, float x0, float y0, float x1, float y1, float zf) {
    addLine(m, {x0, y0, zf}, {x1, y0, zf});
    addLine(m, {x1, y0, zf}, {x1, y1, zf});
    addLine(m, {x1, y1, zf}, {x0, y1, zf});
    addLine(m, {x0, y1, zf}, {x0, y0, zf});
}

Model makeCard(bool adv) {
    Model m;
    const float zf = CARD_D / 2;
    addBox(m, 0, 0, 0, CARD_W, CARD_H, CARD_D);
    addRect(m, -5.3f, 4.6f, 1.5f, 7.4f, zf);  // screen
    addRect(m, 2.5f, 4.6f, 5.3f, 7.4f, zf);   // the stamp module beside it
    addRect(m, -5.6f, 0.6f, 5.6f, 4.0f, zf);  // keyboard outline
    if (adv) addBox(m, -3.0f, CARD_H, 0, 4.0f, 0.8f, 1.2f); // expansion header on top
    m.coarseEdges = static_cast<int>(m.e.size());
    // 4 rows x 14 columns of keys, only drawn up close.
    for (int r = 1; r < 4; ++r) {
        float y = 0.6f + 3.4f * r / 4;
        addLine(m, {-5.6f, y, zf}, {5.6f, y, zf});
    }
    for (int c = 1; c < 14; ++c) {
        float x = -5.6f + 11.2f * c / 14;
        addLine(m, {x, 0.6f, zf}, {x, 4.0f, zf});
    }
    m.radius = 6.5f;
    return m;
}

// The M5Stack Core family: a square body with the screen above three buttons.
// kind 0 = Core Basic, 1 = Core Fire (deeper, LED bars down both sides), 2 = Core2 (larger
// touch screen, three touch marks instead of buttons).
Model makeCore(int kind) {
    Model m;
    const float d = kind == 1 ? CORE_FIRE_D : CORE_D, zf = d / 2;
    addBox(m, 0, 0, 0, CORE_W, CORE_H, d);
    if (kind == 2) addRect(m, -3.6f, 2.4f, 3.6f, 7.5f, zf);
    else addRect(m, -3.4f, 3.0f, 3.4f, 7.3f, zf);
    if (kind == 1) {
        addBox(m, -CORE_W / 2 - 0.25f, 1.5f, 0, 0.5f, 5.0f, 1.0f);
        addBox(m, CORE_W / 2 + 0.25f, 1.5f, 0, 0.5f, 5.0f, 1.0f);
    }
    for (int i = -1; i <= 1; ++i) {
        float x = i * 2.4f;
        if (kind == 2) { // touch marks: small diamonds
            addLine(m, {x - 0.5f, 1.2f, zf}, {x, 1.7f, zf});
            addLine(m, {x, 1.7f, zf}, {x + 0.5f, 1.2f, zf});
            addLine(m, {x + 0.5f, 1.2f, zf}, {x, 0.7f, zf});
            addLine(m, {x, 0.7f, zf}, {x - 0.5f, 1.2f, zf});
        } else {
            addRect(m, x - 0.8f, 0.9f, x + 0.8f, 1.9f, zf);
        }
    }
    m.coarseEdges = static_cast<int>(m.e.size());
    // Up close: the join between the core and its base, round the body.
    float y = kind == 1 ? 2.6f : 0.5f, x0 = -CORE_W / 2, x1 = CORE_W / 2;
    addLine(m, {x0, y, -zf}, {x0, y, zf});
    addLine(m, {x1, y, -zf}, {x1, y, zf});
    addLine(m, {x0, y, -zf}, {x1, y, -zf});
    m.radius = 4.8f;
    return m;
}

Model makeSdCard() {
    Model m;
    const float pts[5][2] = {{-5.5f, 0}, {5.5f, 0}, {5.5f, 12}, {2.5f, 15}, {-5.5f, 15}};
    for (int i = 0; i < 5; ++i) {
        int j = (i + 1) % 5;
        addLine(m, {pts[i][0], pts[i][1], 0.6f}, {pts[j][0], pts[j][1], 0.6f});
        addLine(m, {pts[i][0], pts[i][1], -0.6f}, {pts[j][0], pts[j][1], -0.6f});
        addLine(m, {pts[i][0], pts[i][1], 0.6f}, {pts[i][0], pts[i][1], -0.6f});
    }
    m.coarseEdges = static_cast<int>(m.e.size());
    for (int i = 0; i < 6; ++i) { // contact pads
        float x = -4.2f + i * 1.5f;
        addLine(m, {x, 0.8f, 0.6f}, {x, 3.2f, 0.6f});
    }
    m.radius = 5.5f;
    return m;
}

Model makeGrove() {
    Model m;
    addBox(m, 0, 0, 0, 10, 5, 6);
    m.coarseEdges = static_cast<int>(m.e.size());
    addRect(m, -4, 1, 4, 4, 3);
    for (int i = 0; i < 4; ++i) {
        float x = -3.0f + i * 2.0f;
        addLine(m, {x, 1.6f, 3}, {x, 3.4f, 3});
    }
    m.radius = 6.0f;
    return m;
}

Model makeBrick() {
    Model m;
    addBox(m, 0, 0, 0, 16, 6, 8);
    m.coarseEdges = static_cast<int>(m.e.size());
    for (int i = 0; i < 4; ++i) addBox(m, -6.0f + i * 4.0f, 6, 0, 2.2f, 1.0f, 2.2f);
    m.radius = 9.0f;
    return m;
}

} // namespace

const Model& model(ModelId id) {
    static const Model models[] = {makeCard(false), makeCard(false), makeCard(true),
                                   makeCore(0),     makeCore(1),     makeCore(2),
                                   makeSdCard(),    makeGrove(),     makeBrick()};
    return models[static_cast<int>(id)];
}

} // namespace bz
