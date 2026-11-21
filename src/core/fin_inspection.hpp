#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace eulo {
// Operator-entered, physically *unverified* readings. Distinct from Rocket's
// design fields: importing these cannot alter the flight solver or assert a
// safe fin, CP, servo travel, or a passed inspection.
struct FinReading {
    int number=0;
    double spanMm=0,rootMm=0,tipMm=0,sweepMm=0,thicknessMm=0;
    double setbackMm=0,cantDeg=0,azimuthDeg=0;
};
class FinInspection {
public:
    std::string observedUtc,operatorName,instrument;
    std::vector<FinReading> fins;
    bool load(const char* path,std::string& error);
    // Garage fin-set property indices: span 1, root 2, tip 3, sweep 4,
    // thickness 5, root setback 6. Count/material/density have no numeric
    // per-fin measurement in this file. An index is never a flight rating.
    bool valueForField(int field,size_t finIndex,double& value) const;
    bool rangeForField(int field,double& low,double& high) const;
    double maxAbsCant() const;
};
} // namespace eulo
