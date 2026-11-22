#include "world.h"

namespace bz {

void World::generate(Rng& rng, int count) {
    obstacles.clear();
    const ModelId kinds[] = {ModelId::SdCard, ModelId::Grove, ModelId::Brick};
    for (int tries = 0; tries < count * 40 && static_cast<int>(obstacles.size()) < count; ++tries) {
        float ang = rng.range(-PI, PI), d = rng.range(45.0f, radius - 20.0f);
        float x = std::sin(ang) * d, z = std::cos(ang) * d;
        bool clear = true;
        for (const Obstacle& o : obstacles)
            if (dist2(x, z, o.x, o.z) < 30.0f * 30.0f) clear = false;
        if (!clear) continue;
        ModelId id = kinds[rng.next() % 3];
        obstacles.push_back({id, x, z, rng.range(-PI, PI), model(id).radius});
    }
}

bool World::hitsObstacle(float x, float z, float r) const {
    for (const Obstacle& o : obstacles) {
        float rr = o.radius + r;
        if (dist2(x, z, o.x, o.z) < rr * rr) return true;
    }
    return false;
}

bool World::blocked(float x, float z, float r) const { return !inside(x, z, r) || hitsObstacle(x, z, r); }

} // namespace bz
