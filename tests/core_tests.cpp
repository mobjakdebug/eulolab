#include "core/rocket.hpp"
#include "core/motor.hpp"
#include "core/flight.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

using namespace eulo;
static bool near(double a, double b, double eps=1e-8) { return std::abs(a-b) < eps; }
static MotorCurve from(const char* text) {
    MotorCurve motor;
    std::istringstream stream(text);
    std::string error;
    if (!motor.parse(stream, error)) { std::fprintf(stderr, "%s\n", error.c_str()); std::abort(); }
    return motor;
}
int main() {
    auto motor = from("; test\nDUMMY 18 70 P .01 .02 TEST\n1 10\n2 0\n;");
    assert(motor.points.size() == 3 && motor.points[0].time == 0);
    assert(near(motor.thrust(0.5), 5) && near(motor.thrust(1.5), 5));
    assert(near(motor.thrust(2), 0) && near(motor.totalImpulse(), 10));
    assert(near(motor.impulse(.5), 1.25));
    assert(near(motor.impulse(1.5), 8.75));
    for (const char* malformed : {
        "DUMMY 18 70 P .01 .02 TEST\n1 10\n", // missing zero at burnout
        "DUMMY 18 70 P .01 .02 TEST\n0 0\n1 10\n2 0\n", // explicit origin
        "DUMMY 18 70 P .01 .02 TEST\n1 10\n.5 12\n2 0\n", // unordered times
        "DUMMY 18 70 P .01 .02 TEST\n1 -10\n2 0\n", // negative thrust
        "DUMMY 18 70 P .01 .02 TEST\n1 10\n2 0\n3 10\n", // data after burnout
        "DUMMY 18 70 P .03 .02 TEST\n1 10\n2 0\n", // impossible masses
        "DUMMY 18 70 P .01 nan TEST\n1 10\n2 0\n"
    }) {
        std::istringstream bad(malformed);
        std::string error;
        assert(!motor.parse(bad, error) && !error.empty());
        assert(near(motor.totalImpulse(), 10)); // previous good curve remains intact
    }
    Rocket rocket;
    assert(rocket.valid() && near(rocket.radius,.0125));
    auto mass = massProperties(rocket, .0205, .0096);
    assert(mass.loaded > mass.dry && mass.cgFromTail > 0 && mass.momentPitch > 0);
    rocket.material = Material::Aluminium;
    rocket.bodyDensity = illustrativeDensity(rocket.material);
    assert(massProperties(rocket, .0205, .0096).loaded > mass.loaded);
    rocket.wall = rocket.radius;
    assert(!rocket.valid());
    rocket = Rocket{};
    rocket.motorIndex = 1;
    rocket.parachuteDiameter = .36;
    assert(saveProject(rocket, "out/test-project.ini"));
    Rocket invalid=rocket; invalid.radius=0;
    assert(!saveProject(invalid,"out/test-project.ini"));
    rocket.radius = 0.047;
    assert(loadProject(rocket, "out/test-project.ini"));
    assert(near(rocket.radius, 0.0125) && rocket.motorIndex==1 &&
           near(rocket.parachuteDiameter,.36));
    std::remove("out/test-project.ini");
    {
        FILE* legacy = std::fopen("out/legacy.ini","wb");
        assert(legacy);
        assert(std::fputs("EULOROCKET1\n0.55 0.028 0.002 0.16 0.085 0.08 1280 0\n",legacy)>0);
        assert(std::fclose(legacy)==0);
        assert(loadProject(rocket,"out/legacy.ini") &&
               rocket.parachuteDiameter==0 && rocket.motorIndex==0);
        std::remove("out/legacy.ini");
    }
    rocket = Rocket{};
    MotorCurve c6, d9;
    std::string error;
    assert(c6.load("assets/motors/Klima_C6.eng",error));
    assert(d9.load("assets/motors/Klima_D9.eng",error));
    assert(c6.name=="C6" && d9.name=="D9");
    assert(c6.totalImpulse()>7 && c6.totalImpulse()<11);
    assert(d9.totalImpulse()>15 && d9.totalImpulse()<22);
    auto flight = simulate(rocket,c6);
    assert(flight.valid && flight.apogee > 20 && flight.apogee < 3000);
    assert(flight.apogeeTime > c6.burnTime() && flight.maxG > 1 && flight.samples.size() > 40);
    assert(flight.landed && flight.maxQ > 0 && flight.touchdownSpeed > 0);
    assert(near(flight.at(-1).altitude, 0));
    assert(near(flight.at(flight.apogeeTime).altitude, flight.apogee, .5));
    assert(near(flight.at(1e6).altitude, 0));
    rocket.parachuteDiameter=0;
    auto ballistic=simulate(rocket,c6);
    assert(ballistic.valid && ballistic.landed && ballistic.touchdownSpeed>flight.touchdownSpeed*2);
    rocket.parachuteDiameter=.30;
    rocket.radius=.010; rocket.wall=.002;
    assert(rocket.valid() && !simulate(rocket,c6).valid); // 18 mm motor cannot fit
    rocket=Rocket{};
    auto weak = from("WEAK 18 70 P .01 .02 TEST\n1 0.5\n2 0\n");
    assert(simulate(rocket, weak).valid && simulate(rocket,weak).apogee==0);
    auto falling = simulate(rocket, weak, 50.0);
    assert(falling.valid && falling.at(0).acceleration < 0 && falling.landed);
    std::printf("PASS core: Klima C6 %.1f m model apogee, %.1f m/s model touchdown, %.1f N*s (manufacturer curve; not flight-rated)\n",
        flight.apogee,flight.touchdownSpeed,c6.totalImpulse());
}
