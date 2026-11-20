// Educational vertical point-mass flight model. NOT a stability, structural,
// recovery-reliability or flight-safety analysis. No wind or pitch dynamics.
#include "flight.hpp"
#include <algorithm>
#include <cmath>

namespace eulo {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double g = 9.80665;
constexpr double seaLevelRho = 1.225;
constexpr double scaleHeight = 8500.0; // approximate exponential atmosphere, m
constexpr double stepSeconds = 0.01;
struct State { double h, v; };
struct Forces { double acceleration, thrust, rho, remaining; };
} // namespace

bool motorFits(const Rocket& rocket,const MotorCurve& motor) {
    const double halfLength=motor.lengthMm*0.0005;
    return rocket.valid() && std::isfinite(motor.diameterMm) &&
        std::isfinite(motor.lengthMm) && motor.diameterMm>0 && motor.lengthMm>0 &&
        motor.diameterMm*0.0005 <= rocket.mountBoreRadius &&
        rocket.motorFromTail >= halfLength-1e-9 &&
        rocket.motorFromTail+halfLength <= rocket.mountLength+1e-9;
}

FlightResult simulate(const Rocket& rocket, const MotorCurve& motor, double initialAltitude) {
    FlightResult out;
    if (!motorFits(rocket,motor) || motor.points.size() < 3 || motor.totalImpulse() <= 0 ||
        !std::isfinite(initialAltitude) || initialAltitude < 0 || initialAltitude > 10000 ||
        motor.loadedKg <= motor.propellantKg) return out;
    const MassProperties mass = massProperties(rocket, motor.loadedKg, motor.propellantKg);
    if (mass.loaded <= motor.propellantKg || mass.dry <= 0) return out;
    const double airframeArea = pi * rocket.radius * rocket.radius;
    const double canopyArea = pi * rocket.parachuteDiameter * rocket.parachuteDiameter / 4.0;
    auto forces = [&](double t, const State& state, bool canopyOpen) -> Forces {
        const double consumed = motor.propellantKg *
            std::clamp(motor.impulse(t) / motor.totalImpulse(), 0.0, 1.0);
        const double remaining = motor.propellantKg - consumed;
        const double rho = seaLevelRho * std::exp(-std::max(0.0, state.h) / scaleHeight);
        const double dragArea = rocket.airframeCd * airframeArea +
            (canopyOpen ? rocket.canopyCd * canopyArea : 0.0);
        const double thrust = motor.thrust(t);
        const double drag = -0.5 * rho * dragArea * state.v * std::fabs(state.v);
        return {(thrust + drag) / (mass.dry + remaining) - g, thrust, rho, remaining};
    };
    auto acceleration = [&](double t, State state, bool canopyOpen) {
        return forces(t,state,canopyOpen).acceleration;
    };
    auto sample = [&](double t, State state, bool canopyOpen) -> FlightSample {
        const Forces f = forces(t,state,canopyOpen);
        double a = f.acceleration;
        if (state.h <= 0 && state.v <= 0 && a <= 0) a = 0; // launch rail / ground
        return {t, state.h, state.v, a, f.thrust, std::fabs(a + g) / g,
            0.5 * f.rho * state.v * state.v, f.remaining};
    };
    State s{initialAltitude, 0.0};
    bool airborne = initialAltitude > 0;
    bool canopyOpen = false;
    out.valid = true;
    out.apogee = initialAltitude;
    out.samples.reserve(6200);
    for (int i = 0; i <= 30000; ++i) {
        const double t = i * stepSeconds;
        // Ideal instantaneous apogee deployment only. The motor's delay field,
        // ejection, attachment strength, inflation and failures are NOT modelled.
        if (airborne && s.v < 0 && rocket.parachuteDiameter > 0) canopyOpen = true;
        const FlightSample current = sample(t,s,canopyOpen);
        if (i % 5 == 0 || i == 30000) out.samples.push_back(current);
        if (s.h > out.apogee) { out.apogee = s.h; out.apogeeTime = t; }
        out.maxSpeed = std::max(out.maxSpeed,std::fabs(s.v));
        out.maxG = std::max(out.maxG,current.properG);
        out.maxQ = std::max(out.maxQ,current.dynamicPressure);
        if (!airborne && t > motor.burnTime() + 0.5) break;
        if (!airborne && current.acceleration <= 0) continue;
        // Fixed 10 ms Runge-Kutta fourth-order integration. At the ground the
        // constraint is checked before integrating; no negative altitude is kept.
        const double a1 = acceleration(t,s,canopyOpen);
        const double v2 = s.v + 0.5*stepSeconds*a1;
        const double a2 = acceleration(t+0.5*stepSeconds,
            {s.h+0.5*stepSeconds*s.v,v2},canopyOpen);
        const double v3 = s.v + 0.5*stepSeconds*a2;
        const double a3 = acceleration(t+0.5*stepSeconds,
            {s.h+0.5*stepSeconds*v2,v3},canopyOpen);
        const double v4 = s.v + stepSeconds*a3;
        const double a4 = acceleration(t+stepSeconds,
            {s.h+stepSeconds*v3,v4},canopyOpen);
        State next{s.h+stepSeconds*(s.v+2*v2+2*v3+v4)/6.0,
                   s.v+stepSeconds*(a1+2*a2+2*a3+a4)/6.0};
        if (next.h > 0) airborne = true;
        if (s.v > 0 && next.v <= 0) {
            const double fraction = s.v / (s.v-next.v);
            const double dt = stepSeconds * fraction;
            const double apex = s.h + s.v*dt + 0.5*a1*dt*dt;
            if (apex >= out.apogee) {out.apogee=apex;out.apogeeTime=t+dt;}
        }
        if (next.h <= 0 && airborne) {
            const double fraction = std::clamp(s.h/(s.h-next.h),0.0,1.0);
            const double touchdown = t+stepSeconds*fraction;
            out.touchdownSpeed = std::fabs(s.v+(next.v-s.v)*fraction);
            out.maxSpeed = std::max(out.maxSpeed,out.touchdownSpeed);
            out.landed = true;
            out.samples.push_back(sample(touchdown,{0,0},canopyOpen));
            break;
        }
        s=next;
    }
    return out;
}

FlightSample FlightResult::at(double time) const {
    if (samples.empty()) return {};
    if (time <= samples.front().time) return samples.front();
    if (time >= samples.back().time) return samples.back();
    const auto it = std::lower_bound(samples.begin(), samples.end(), time,
        [](const FlightSample& s, double t) { return s.time < t; });
    const auto& a = *(it - 1);
    const auto& b = *it;
    const double f = (time - a.time) / (b.time - a.time);
    return {time, a.altitude + (b.altitude-a.altitude)*f,
        a.velocity + (b.velocity-a.velocity)*f,
        a.acceleration + (b.acceleration-a.acceleration)*f,
        a.thrust + (b.thrust-a.thrust)*f,
        a.properG + (b.properG-a.properG)*f,
        a.dynamicPressure + (b.dynamicPressure-a.dynamicPressure)*f,
        a.propellantRemaining + (b.propellantRemaining-a.propellantRemaining)*f};
}
} // namespace eulo
