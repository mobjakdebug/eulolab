#include "rocket.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace eulo {
namespace {
constexpr double pi = 3.14159265358979323846;
struct Part { double mass, x, intrinsicI; };
}

const char* materialName(Material material) {
    switch (material) {
        case Material::PVC: return "PVC";
        case Material::Aluminium: return "Aluminium";
        case Material::CarbonLaminate: return "Carbon laminate";
    }
    return "Unknown";
}

double illustrativeDensity(Material material) {
    switch (material) {
        case Material::PVC: return 1280.0;
        case Material::Aluminium: return 2700.0;
        case Material::CarbonLaminate: return 1500.0;
    }
    return 0.0;
}

bool Rocket::valid() const {
    return std::isfinite(bodyLength) && bodyLength >= 0.2 && bodyLength <= 3.0 &&
           std::isfinite(radius) && radius >= 0.01 && radius <= 0.1 &&
           std::isfinite(wall) && wall > 0.0003 && wall < radius * 0.5 &&
           std::isfinite(noseLength) && noseLength > radius && noseLength < bodyLength &&
           std::isfinite(finSpan) && finSpan >= 0.02 && finSpan <= 0.25 &&
           std::isfinite(electronicsMass) && electronicsMass >= 0 && electronicsMass < 2 &&
           std::isfinite(bodyDensity) && bodyDensity >= 100 && bodyDensity <= 8000 &&
           std::isfinite(parachuteDiameter) && (parachuteDiameter == 0 ||
               (parachuteDiameter >= 0.1 && parachuteDiameter <= 1.5)) &&
           static_cast<int>(material) >= 0 && static_cast<int>(material) <= 2 &&
           motorIndex >= 0 && motorIndex <= 1;
}

MassProperties massProperties(const Rocket& r, double motorMass, double propellantMass) {
    MassProperties result;
    if (!r.valid() || !std::isfinite(motorMass) || !std::isfinite(propellantMass) ||
        motorMass < 0 || propellantMass < 0 || propellantMass > motorMass) return result;
    const double inner = r.radius - r.wall;
    const double bodyMass = pi * (r.radius * r.radius - inner * inner) * r.bodyLength * r.bodyDensity;
    // A thin cone shell and three uniform trapezoidal fins: first-order mass model.
    const double noseSlant = std::hypot(r.radius, r.noseLength);
    const double noseMass = pi * r.radius * noseSlant * r.wall * r.bodyDensity;
    // Illustrative balsa fins (three trapezoids) and canopy fabric + cord.
    // Neither these densities nor recovery performance are material certificates.
    const double finRoot = std::min(0.10, r.bodyLength * 0.35);
    const double finMass = 3.0 * 0.5 * (finRoot + finRoot * 0.55) *
        r.finSpan * 0.0025 * 210.0;
    const double canopyMass = r.parachuteDiameter > 0 ?
        pi * r.parachuteDiameter * r.parachuteDiameter / 4.0 * 0.09 + 0.002 : 0.0;
    const Part parts[] = {
        {bodyMass, r.bodyLength * 0.5, bodyMass * r.bodyLength * r.bodyLength / 12.0},
        {noseMass, r.bodyLength + r.noseLength / 3.0, noseMass * r.noseLength * r.noseLength / 18.0},
        {finMass, finRoot * 0.5, finMass * finRoot * finRoot / 12.0},
        {r.electronicsMass, r.bodyLength * 0.66, 0.0},
        {canopyMass, r.bodyLength * 0.80, 0.0},
        {motorMass, 0.035, motorMass * 0.07 * 0.07 / 12.0}
    };
    double weighted = 0.0;
    for (const Part& part : parts) {
        result.loaded += part.mass;
        weighted += part.mass * part.x;
    }
    result.dry = result.loaded - propellantMass;
    if (result.loaded > 0.0) {
        result.cgFromTail = weighted / result.loaded;
        for (const Part& part : parts) {
            const double dx = part.x - result.cgFromTail;
            result.momentPitch += part.intrinsicI + part.mass * dx * dx;
        }
    }
    return result;
}

bool saveProject(const Rocket& rocket, const char* path) {
    if (!rocket.valid() || !path) return false;
    // Write beside the project, then rename it. A failed or interrupted write
    // must not truncate the user's previously saved design.
    const std::string temporary = std::string(path) + ".tmp";
    FILE* f = std::fopen(temporary.c_str(), "wb");
    if (!f) return false;
    const int count = std::fprintf(f, "EULOROCKET2\n%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %d %d\n",
        rocket.bodyLength, rocket.radius, rocket.wall, rocket.noseLength,
        rocket.finSpan, rocket.electronicsMass, rocket.bodyDensity,
        rocket.parachuteDiameter, static_cast<int>(rocket.material), rocket.motorIndex);
    const int closeResult = std::fclose(f);
    if (count <= 0 || closeResult != 0 || std::rename(temporary.c_str(),path) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

bool loadProject(Rocket& rocket, const char* path) {
    if (!path) return false;
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char header[32] = {};
    Rocket candidate;
    int material = -1;
    bool ok = std::fgets(header, sizeof(header), f) != nullptr;
    if (ok && std::strcmp(header, "EULOROCKET2\n") == 0) {
        ok = std::fscanf(f, "%lf %lf %lf %lf %lf %lf %lf %lf %d %d",
            &candidate.bodyLength, &candidate.radius, &candidate.wall,
            &candidate.noseLength, &candidate.finSpan, &candidate.electronicsMass,
            &candidate.bodyDensity, &candidate.parachuteDiameter, &material,
            &candidate.motorIndex) == 10;
    } else if (ok && std::strcmp(header, "EULOROCKET1\n") == 0) {
        candidate.parachuteDiameter = 0; // old projects had no recovery model
        candidate.motorIndex = 0;
        ok = std::fscanf(f, "%lf %lf %lf %lf %lf %lf %lf %d",
            &candidate.bodyLength, &candidate.radius, &candidate.wall,
            &candidate.noseLength, &candidate.finSpan, &candidate.electronicsMass,
            &candidate.bodyDensity, &material) == 8;
    } else ok = false;
    std::fclose(f);
    if (!ok || material < 0 || material > 2) return false;
    candidate.material = static_cast<Material>(material);
    if (!candidate.valid()) return false;
    rocket = candidate;
    return true;
}

}  // namespace eulo
