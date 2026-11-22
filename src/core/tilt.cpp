#include "tilt.h"

namespace bz {

void Tilt::update(const float a[3], int axis, int sign, float dt) {
    float mag = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    if (mag < 1e-4f || axis < 0 || axis > 2) return;
    // Angle of the left-right axis from horizontal. This does not depend on how far the
    // device is pitched towards the player, only on how far it is rolled.
    float raw = std::asin(clampf(a[axis] / mag, -1.0f, 1.0f)) * static_cast<float>(sign);
    if (!primed) {
        filtered = raw;
        primed = true;
        return;
    }
    filtered += (raw - filtered) * (1.0f - std::exp(-dt / tau));
}

float Tilt::steer() const {
    float d = angle();
    float m = std::fabs(d);
    if (m <= deadZone) return 0.0f;
    float t = (m - deadZone) / (fullAngle - deadZone);
    if (t > 1.0f) t = 1.0f;
    t = t * (0.6f + 0.4f * t); // gentler near the centre
    return d < 0 ? -t : t;
}

} // namespace bz
