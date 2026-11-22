// First-person camera: world -> camera space -> screen, with near-plane clipping.
#pragma once
#include "raster.h"
#include "vec.h"

namespace bz {

constexpr float FOCAL = 277.0f; // about a 60 degree horizontal field of view at 320 px
constexpr float NEAR_Z = 0.5f;
constexpr float VIEW_CX = SCREEN_W / 2.0f;
constexpr float VIEW_CY = 92.0f; // the horizon row

struct Camera {
    float x = 0, z = 0, eye = 0, s = 0, c = 1;

    void set(float px, float pz, float heading, float eyeHeight) {
        x = px;
        z = pz;
        eye = eyeHeight;
        s = std::sin(heading);
        c = std::cos(heading);
    }

    // Camera space: x right, y up, z forward.
    Vec3 toCam(Vec3 p) const {
        float dx = p.x - x, dz = p.z - z;
        return {dx * c - dz * s, p.y - eye, dx * s + dz * c};
    }
};

inline bool projectPoint(const Camera& cam, Vec3 p, float& sx, float& sy) {
    Vec3 q = cam.toCam(p);
    if (q.z < NEAR_Z) return false;
    sx = VIEW_CX + FOCAL * q.x / q.z;
    sy = VIEW_CY - FOCAL * q.y / q.z;
    return true;
}

// Projects a world segment to screen x0,y0,x1,y1. Returns false if it is behind the camera.
inline bool projectSegment(const Camera& cam, Vec3 a, Vec3 b, float out[4]) {
    Vec3 ca = cam.toCam(a), cb = cam.toCam(b);
    if (ca.z < NEAR_Z && cb.z < NEAR_Z) return false;
    if (ca.z < NEAR_Z) {
        float t = (NEAR_Z - ca.z) / (cb.z - ca.z);
        ca = {ca.x + (cb.x - ca.x) * t, ca.y + (cb.y - ca.y) * t, NEAR_Z};
    } else if (cb.z < NEAR_Z) {
        float t = (NEAR_Z - cb.z) / (ca.z - cb.z);
        cb = {cb.x + (ca.x - cb.x) * t, cb.y + (ca.y - cb.y) * t, NEAR_Z};
    }
    out[0] = VIEW_CX + FOCAL * ca.x / ca.z;
    out[1] = VIEW_CY - FOCAL * ca.y / ca.z;
    out[2] = VIEW_CX + FOCAL * cb.x / cb.z;
    out[3] = VIEW_CY - FOCAL * cb.y / cb.z;
    return true;
}

} // namespace bz
