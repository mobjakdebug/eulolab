// EuloLab educational model-rocket simulator core (MIT, see LICENSE).
#pragma once
#include <string>

namespace eulo {

enum class Material : int { PVC = 0, Aluminium = 1, CarbonLaminate = 2 };
enum class NoseProfile : int { Cone = 0, TangentOgive = 1 };
enum class FinMaterial : int { Balsa = 0, Plywood = 1, CarbonLaminate = 2 };
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
    bool valid() const;
};

// Fraction is 0 at the shoulder and 1 at the tip.
double noseRadius(const Rocket& rocket, double fraction);

struct MassProperties {
    double loaded = 0.0;
    double dry = 0.0;
    double cgFromTail = 0.0;
    double momentPitch = 0.0;
};

MassProperties massProperties(const Rocket& rocket, double loadedMotorMass,
                              double propellantMass);
bool saveProject(const Rocket& rocket, const char* path);
bool loadProject(Rocket& rocket, const char* path);

} // namespace eulo
