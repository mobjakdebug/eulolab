#pragma once
#include "canvas.hpp"
#include "core/flight.hpp"
#include "core/mesh.hpp"
#include "core/telemetry.hpp"

namespace eulo {
struct RocketView {
    int x = 260, y = 138, w = 640, h = 444;
    double yaw = 0.65;
    double pitch = 0.12;
    double zoom = 1.0;
    double altitude = 0.0;
    double thrust = 0.0;
    bool flight = false;
    bool canopyOpen = false; // visualisation of idealised apogee model only
    int selected = -1;
    int cameraPreset = 0; // 0: iso, 1: side, 2: top, 3: manually orbited
    const FlightResult* trajectory = nullptr;
    const ReferenceMesh* reference = nullptr;
    FlightSample sample{};
};
void drawRocketView(Canvas& canvas, const Rocket& rocket,
                    const MassProperties& mass, const RocketView& view);
void drawRecordedView(Canvas& canvas, const RocketView& view,
                      const TelemetrySample& sample, bool calibrated,
                      double relativeAltitudeM, bool hasPressure);
}
