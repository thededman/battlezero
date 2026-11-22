// The board: a round arena scattered with oversized hardware.
#pragma once
#include <cstdint>
#include <vector>

#include "models.h"

namespace bz {

// Small deterministic generator so runs can be replayed in tests and frame dumps.
struct Rng {
    uint32_t s = 1;
    uint32_t next() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    }
    float uniform() { return static_cast<float>(next() & 0xFFFF) / 65536.0f; }
    float range(float a, float b) { return a + (b - a) * uniform(); }
};

struct Obstacle {
    ModelId id;
    float x, z, heading, radius;
};

struct World {
    float radius = 300.0f;
    std::vector<Obstacle> obstacles;

    void generate(Rng& rng, int count = 28);
    // True if a circle at (x, z) overlaps an obstacle or leaves the board.
    bool blocked(float x, float z, float r) const;
    bool hitsObstacle(float x, float z, float r) const;
    bool inside(float x, float z, float r) const { return x * x + z * z <= (radius - r) * (radius - r); }
};

} // namespace bz
