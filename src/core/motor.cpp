#include "motor.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace eulo {
bool MotorCurve::parse(std::istream& in, std::string& error) {
    // Never publish a partially parsed curve to the physics engine.
    MotorCurve candidate;
    bool header = false;
    bool ended = false;
    std::string line;
    unsigned lineNumber = 0;
    while (std::getline(in, line)) {
        ++lineNumber;
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == ';') continue;
        std::istringstream row(line.substr(first));
        if (!header) {
            std::string extra;
            if (!(row >> candidate.name >> candidate.diameterMm >> candidate.lengthMm >>
                  candidate.delays >> candidate.propellantKg >> candidate.loadedKg >>
                  candidate.manufacturer) || (row >> extra) ||
                !std::isfinite(candidate.diameterMm) || !std::isfinite(candidate.lengthMm) ||
                !std::isfinite(candidate.propellantKg) || !std::isfinite(candidate.loadedKg) ||
                candidate.diameterMm <= 0 || candidate.lengthMm <= 0 ||
                candidate.propellantKg < 0 || candidate.loadedKg <= candidate.propellantKg) {
                error = "Invalid RASP header at line " + std::to_string(lineNumber);
                return false;
            }
            candidate.points.push_back({0.0, 0.0}); // implicit RASP origin
            header = true;
            continue;
        }
        double time = 0, force = 0;
        std::string extra;
        if (ended || !(row >> time >> force) || (row >> extra) ||
            !std::isfinite(time) || !std::isfinite(force) ||
            time <= candidate.points.back().time || force < 0 ||
            candidate.points.size() >= 2048) {
            error = "Invalid thrust point at line " + std::to_string(lineNumber);
            return false;
        }
        candidate.points.push_back({time, force});
        if (force == 0.0) ended = true;
    }
    if (!header || !ended || candidate.points.size() < 3) {
        error = "Motor curve needs a header, positive thrust and a final zero-thrust point";
        return false;
    }
    candidate.cumulative_.assign(candidate.points.size(), 0.0);
    for (size_t i = 1; i < candidate.points.size(); ++i) {
        const auto& a = candidate.points[i - 1];
        const auto& b = candidate.points[i];
        candidate.cumulative_[i] = candidate.cumulative_[i - 1] +
            0.5 * (b.newtons + a.newtons) * (b.time - a.time);
    }
    if (!std::isfinite(candidate.totalImpulse()) || candidate.totalImpulse() <= 0) {
        error = "Motor total impulse must be positive and finite";
        return false;
    }
    *this = std::move(candidate);
    error.clear();
    return true;
}

bool MotorCurve::load(const char* path, std::string& error) {
    std::ifstream in(path);
    if (!in) { error = "Cannot open .eng file"; return false; }
    return parse(in, error);
}

double MotorCurve::thrust(double t) const {
    if (points.size() < 2 || t < 0 || t >= points.back().time) return 0;
    const auto it = std::upper_bound(points.begin(), points.end(), t,
        [](double time, const ThrustPoint& p) { return time < p.time; });
    if (it == points.begin() || it == points.end()) return 0;
    const auto& b = *it;
    const auto& a = *(it - 1);
    const double weight = (t - a.time) / (b.time - a.time);
    return a.newtons + weight * (b.newtons - a.newtons);
}

double MotorCurve::impulse(double t) const {
    if (points.size() < 2 || t <= 0) return 0;
    if (t >= burnTime()) return totalImpulse();
    const auto it = std::upper_bound(points.begin(), points.end(), t,
        [](double time, const ThrustPoint& p) { return time < p.time; });
    const size_t prev = static_cast<size_t>((it - points.begin()) - 1);
    const double h = t - points[prev].time;
    return cumulative_[prev] + 0.5 * h * (points[prev].newtons + thrust(t));
}

double MotorCurve::totalImpulse() const {
    return cumulative_.empty() ? 0.0 : cumulative_.back();
}
}  // namespace eulo
