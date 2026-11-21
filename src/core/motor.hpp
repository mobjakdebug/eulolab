// RASP .eng reader for public commercial-motor DATA (not propellant recipes).
#pragma once
#include <istream>
#include <string>
#include <vector>

namespace eulo {
struct ThrustPoint { double time, newtons; };

class MotorCurve {
public:
    std::string name;
    std::string manufacturer;
    std::string delays;
    double diameterMm = 0.0;
    double lengthMm = 0.0;
    double propellantKg = 0.0;
    double loadedKg = 0.0;
    std::vector<ThrustPoint> points;

    bool parse(std::istream& in, std::string& error);
    bool load(const char* path, std::string& error);
    double thrust(double time) const;
    double impulse(double time) const;
    double totalImpulse() const;
    double burnTime() const { return points.empty() ? 0.0 : points.back().time; }
private:
    std::vector<double> cumulative_;
};
}  // namespace eulo
