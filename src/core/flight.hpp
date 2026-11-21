// Three-dimensional virtual rigid-body experiment. Illustrative coefficients;
// NOT flight-safety, stability certification, a real controller or actuator IO.
#pragma once
#include "motor.hpp"
#include "rocket.hpp"
#include <vector>

namespace eulo {
// World axes: x=east, y=up (metres), z=north. Body +Y points toward the nose.
struct Vec3 { double x=0,y=0,z=0; };
struct Quaternion { double w=1,x=0,y=0,z=0; };
struct FlightSample {
    double time=0,altitude=0,velocity=0,acceleration=0;
    double thrust=0,properG=0,dynamicPressure=0,propellantRemaining=0;
    Vec3 position{},velocityWorld{},angularVelocity{};
    Quaternion orientation{};
    double speed=0,mach=0,range=0,tiltDeg=0,headingDeg=0;
    bool canopyOpen=false;
};
struct FlightResult {
    std::vector<FlightSample> samples;
    double apogee=0,apogeeTime=0,maxSpeed=0,maxG=0,maxQ=0,maxMach=0;
    double maxRange=0,maxTiltDeg=0;
    double touchdownSpeed=0; // speed immediately BEFORE the ground constraint
    bool valid=false,landed=false;
    FlightSample at(double time) const;
};
// The selected commercial motor must fit entirely inside the editable sleeve.
bool motorFits(const Rocket& rocket,const MotorCurve& motor);
// Open-loop gimbal and fin-sideforce assumptions are intentionally visible in
// the project. No autopilot, feedback controller or physical servo commands.
FlightResult simulate(const Rocket& rocket,const MotorCurve& motor,
                      double initialAltitude=0.0);
} // namespace eulo
