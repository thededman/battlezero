// Wireframe meshes. Local space: x across, y up from the ground, +z is the front face.
#pragma once
#include <cstdint>
#include <utility>
#include <vector>

#include "vec.h"

namespace bz {

enum class ModelId { CardK132, CardV11, CardAdv, SdCard, Grove, Brick, COUNT };

struct Model {
    std::vector<Vec3> v;
    std::vector<std::pair<uint16_t, uint16_t>> e;
    int coarseEdges = 0; // edges [0, coarseEdges) are drawn at any distance
    float radius = 1;    // collision circle on the ground
};

const Model& model(ModelId id);

// Card body size shared by every Cardputer model (84 x 54 x 20 mm, scaled up).
constexpr float CARD_W = 12.6f, CARD_H = 8.1f, CARD_D = 3.0f;

} // namespace bz
