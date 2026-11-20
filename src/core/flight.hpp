// One-dimensional educational flight model; NOT flight-safety analysis.
#pragma once
#include "motor.hpp"
#include "rocket.hpp"
#include <vector>

namespace eulo {
struct FlightSample {
    double time = 0, altitude = 0, velocity = 0, acceleration = 0;
    double thrust = 0, properG = 0, dynamicPressure = 0, propellantRemaining = 0;
};
struct FlightResult {
    std::vector<FlightSample> samples;
    double apogee = 0, apogeeTime = 0, maxSpeed = 0, maxG = 0, maxQ = 0;
    double touchdownSpeed = 0; // magnitude immediately BEFORE the ground constraint
    bool valid = false, landed = false;
    FlightSample at(double time) const;
};
// Catalog motor must fit inside the editable internal sleeve, with its whole
// casing between the tail datum and the top of the mount.
bool motorFits(const Rocket& rocket,const MotorCurve& motor);
FlightResult simulate(const Rocket& rocket, const MotorCurve& motor,
                      double initialAltitude = 0.0);
}  // namespace eulo
