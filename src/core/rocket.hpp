// EuloLab educational model-rocket simulator core (MIT, see LICENSE).
#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace eulo {

enum class Material : int { PVC = 0, Aluminium = 1, CarbonLaminate = 2 };
enum class NoseProfile : int { Cone = 0, TangentOgive = 1 };
enum class FinMaterial : int { Balsa = 0, Plywood = 1, CarbonLaminate = 2 };
// Source of each editable MODEL input, never a certification of measurement.
// Old project formats had no provenance and must not be called user-entered.
enum class InputOrigin : uint8_t { Template=0, UserDesign=1, LegacyUnknown=2 };
const char* materialName(Material material);
const char* finMaterialName(FinMaterial material);
const char* noseProfileName(NoseProfile profile);
double illustrativeDensity(Material material);
double illustrativeFinDensity(FinMaterial material);

// SI throughout. Editable parameters are illustrative, not part specifications
// or a validated stability, structural or recovery analysis.
struct Rocket {
    double bodyLength = 0.34;
    double radius = 0.0125;
    double wall = 0.0009;
    Material material = Material::PVC;
    double bodyDensity = 1280.0;
    double airframeCd = 0.65;

    double noseLength = 0.095;
    double noseWall = 0.0009;
    NoseProfile noseProfile = NoseProfile::Cone;
    Material noseMaterial = Material::PVC;
    double noseDensity = 1280.0;

    int finCount = 3;
    double finSpan = 0.048;
    double finRootChord = 0.095;
    double finTipChord = 0.052;
    double finSweep = 0.024;
    double finThickness = 0.0025;
    double finSetback = 0.006;
    FinMaterial finMaterial = FinMaterial::Balsa;
    double finDensity = 210.0;

    double electronicsMass = 0.015;
    double electronicsFromTail = 0.2244;
    int motorIndex = 0; // published Klima C6/D9 curves, never an invented motor
    double motorFromTail = 0.035;
    // Internal sleeve around the selected motor, concentric with the airframe.
    double mountLength = 0.075;
    double mountBoreRadius = 0.0095; // 19 mm bore for an 18 mm published motor
    double mountWall = 0.0009;
    double mountDensity = 1280.0;

    double parachuteDiameter = 0.30; // 0 disables idealised recovery
    double canopyCd = 1.20;
    double canopyArealDensity = 0.09;
    double recoveryFromTail = 0.272;

    // One coaxial, editable insert. The bore can be zero (solid cylinder).
    // Its center is measured from the tail. It must clear the motor sleeve.
    bool insertEnabled = false;
    double insertLength = 0.030;
    double insertOuterRadius = 0.006;
    double insertBoreRadius = 0.0;
    double insertFromTail = 0.19;
    Material insertMaterial = Material::PVC;
    double insertDensity = 1280.0;

    // Virtual flight setup. These are illustrative scenario assumptions, not
    // commands sent to an actuator or a guidance solution for a real vehicle.
    double launchTiltDeg = 2.0;
    double launchHeadingDeg = 0.0;
    double windEast = 1.5;
    double windNorth = 0.4;
    double gimbalPitchDeg = 0.0;
    double gimbalYawDeg = 0.0;
    double cpFromTail = 0.09;
    double angularDamping = 0.002;

    // Linked visual reference beside the .eul file. Meshes are neither solids
    // nor included in mass, aerodynamics or the flight solver.
    int referenceFormat = 0; // 0 none, 1 reference.obj, 2 reference.stl
    double referenceUnitMm = 1.0; // mm per imported vertex unit
    double referenceEastMm = 0.0;
    double referenceTailMm = 0.0;
    double referenceNorthMm = 0.0;
    static constexpr int inputCount=52;
    std::array<InputOrigin,inputCount> inputOrigins{}; // defaults = Template
    InputOrigin origin(int part,int field) const;
    bool markOrigin(int part,int field,InputOrigin source);
    bool valid() const;
};

// Fraction is 0 at the shoulder and 1 at the tip.
double noseRadius(const Rocket& rocket, double fraction);

struct MassProperties {
    double loaded = 0.0;
    double dry = 0.0;
    double cgFromTail = 0.0;
    double momentPitch = 0.0;
    double momentRoll = 0.0; // approximate polar moment, kg*m^2
};

MassProperties massProperties(const Rocket& rocket, double loadedMotorMass,
                              double propellantMass);
bool saveProject(const Rocket& rocket, const char* path);
bool loadProject(Rocket& rocket, const char* path);

} // namespace eulo
