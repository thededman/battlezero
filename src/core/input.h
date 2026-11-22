// What a platform front end hands the game each frame. All keys are "held" states;
// the game works out presses itself.
#pragma once

namespace bz {

struct Input {
    bool left = false, right = false; // steer by key
    bool forward = false, reverse = false;
    bool fire = false;
    bool pause = false;      // ESC
    bool recenter = false;   // R
    bool tiltToggle = false; // T
    bool invert = false;     // I, calibration screen
    bool axis = false;       // A, calibration screen
    bool mute = false;       // M
    bool tiltAvailable = false;
    float accel[3] = {0, 0, 1}; // any consistent unit; only the direction is used
};

} // namespace bz
