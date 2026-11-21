#pragma once
#include "flight.hpp"
#include <string>
#include <vector>

namespace eulo {
// Recorded gyro rates use the sensor's body axes in deg/s; altitude is the
// logger's metres. An initial identity quaternion is a relative reference,
// not an absolute pose or a reconstructed position.
struct TelemetrySample {
    double time=0,altitude=0;
    Vec3 gyro{};
    Quaternion attitude{};
};
class TelemetryRecording {
public:
    std::vector<TelemetrySample> samples;
    Vec3 gyroBias{}; // mean of the stationary calibration window, deg/s
    double altitudeZero=0; // mean altitude of that same window, metres
    bool calibrated=false;
    bool load(const char* path,std::string& error);
    bool calibrate(std::string& error);
    TelemetrySample at(double time) const;
};
} // namespace eulo
