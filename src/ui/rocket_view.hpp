#pragma once
#include "canvas.hpp"
#include "core/rocket.hpp"

namespace eulo {
struct RocketView {
    int x = 260, y = 138, w = 640, h = 444;
    double yaw = 0.65;
    double altitude = 0.0;
    double thrust = 0.0;
    bool flight = false;
    bool canopyOpen = false; // visualisation of idealised apogee model only
    int selected = -1;
};
void drawRocketView(Canvas& canvas, const Rocket& rocket,
                    const MassProperties& mass, const RocketView& view);
}
