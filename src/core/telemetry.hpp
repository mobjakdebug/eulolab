#pragma once
#include "flight.hpp"
#include <string>
#include <vector>

namespace eulo {
// A CSV is user-supplied data, not proof that a sensor was attached. Legacy
// files supply an altitude in metres; V2 files contain actual raw barometer +
// IMU columns (if produced by an instrumented logger). We never infer GPS
// position, absolute altitude, servo position, or an absolute orientation.
enum class RecordingFormat { LegacyAltitude, BarometerImu };
struct TelemetrySample {
    double time=0,altitude=0; // v2 altitude is derived only AFTER calibration
    double pressurePa=0,temperatureC=0; // v2 only
    Vec3 gyro{}; // sensor body axes, deg/s
    Vec3 acceleration{}; // v2 only, m/s^2; NOT integrated to position
    Quaternion attitude{};
    bool attitudeValid=false; // false across missing samples or before calibration
};
class TelemetryRecording {
public:
    std::vector<TelemetrySample> samples;
    RecordingFormat format=RecordingFormat::LegacyAltitude;
    Vec3 gyroBias{}; // mean of the stationary calibration window, deg/s
    double altitudeZero=0; // legacy mean altitude, metres
    double pressureZeroPa=0; // v2 stationary reference pressure, Pa
    bool calibrated=false;
    bool hasPressure() const {return format==RecordingFormat::BarometerImu;}
    // Only call for calibrated samples; relative pressure altitude is an ISA
    // approximation, not above-sea-level or GPS altitude.
    double relativeAltitude(const TelemetrySample& sample) const {
        return sample.altitude-altitudeZero;
    }
    bool load(const char* path,std::string& error);
    bool calibrate(std::string& error);
    TelemetrySample at(double time) const;
};
} // namespace eulo
