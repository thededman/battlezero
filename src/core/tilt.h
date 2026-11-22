// Turns an accelerometer reading into a steering value in -1..1.
#pragma once
#include "vec.h"

namespace bz {

struct Tilt {
    float deadZone = 4.0f * PI / 180.0f;  // no steering inside this
    float fullAngle = 25.0f * PI / 180.0f; // full turn rate from here on
    float tau = 0.08f;                     // low-pass time constant, seconds

    // axis: which accelerometer axis runs left-right across the device (0..2).
    // sign: +1 or -1 so that tilting right gives a positive angle.
    void update(const float accel[3], int axis, int sign, float dt);
    void recenter() { zero = filtered; }
    float angle() const { return filtered - zero; } // radians, right is positive
    float steer() const;

    float zero = 0, filtered = 0;
    bool primed = false;
};

} // namespace bz
