#pragma once
#include "flight.hpp"
#include <string>
#include <vector>

namespace eulo {
enum class Surface { Body, Nose, Fin, Insert, Reference };
struct MeshTriangle {
    Vec3 a{},b{},c{};
    Surface surface=Surface::Reference;
    double angle=0; // local radial angle for the renderer's simple lighting
};
// Shared visual triangles for the viewport and OBJ export. Shell thickness,
// internal assembly and fin thickness are NOT represented as solid geometry.
std::vector<MeshTriangle> visualMesh(const Rocket& rocket);
class ReferenceMesh {
public:
    std::vector<MeshTriangle> triangles;
    Vec3 lower{},upper{}; // imported units, not metres
    bool load(const char* path,std::string& error); // OBJ, ASCII or binary STL
};
bool exportVisualObj(const Rocket& rocket,const char* path);
} // namespace eulo
