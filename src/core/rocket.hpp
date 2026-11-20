// EuloLab - educational model-rocket simulator core (MIT, see LICENSE).
#pragma once
#include <string>

namespace eulo {

enum class Material : int { PVC = 0, Aluminium = 1, CarbonLaminate = 2 };
const char* materialName(Material material);
double illustrativeDensity(Material material);

// SI throughout: metres, kilograms, seconds and newtons. Densities are
// editable, illustrative inputs and NOT certified properties of actual parts.
struct Rocket {
    // An illustrative 18 mm commercial-motor model rocket. Editable parts
    // are not a construction plan, certified stability analysis, or launch advice.
    double bodyLength = 0.34;
    double radius = 0.0125;
    double wall = 0.0009;
    double noseLength = 0.095;
    double finSpan = 0.048;
    double electronicsMass = 0.015;
    double bodyDensity = 1280.0;
    double parachuteDiameter = 0.30; // 0 disables idealised recovery
    Material material = Material::PVC;
    int motorIndex = 0; // 0 = Klima C6, 1 = Klima D9, from public-domain .eng data
    bool valid() const;
};

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

}  // namespace eulo
