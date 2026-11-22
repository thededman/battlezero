// Small maths helpers shared by the whole core.
#pragma once
#include <cmath>

namespace bz {

constexpr float PI = 3.14159265358979f;

struct Vec3 {
    float x, y, z;
};

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Wraps an angle to (-PI, PI].
inline float wrapAngle(float a) {
    while (a > PI) a -= 2 * PI;
    while (a <= -PI) a += 2 * PI;
    return a;
}

// Heading convention: 0 faces +z, positive turns right (towards +x).
inline float bearingTo(float fromX, float fromZ, float toX, float toZ) {
    return std::atan2(toX - fromX, toZ - fromZ);
}

inline float dist2(float ax, float az, float bx, float bz_) {
    float dx = ax - bx, dz = az - bz_;
    return dx * dx + dz * dz;
}

} // namespace bz
