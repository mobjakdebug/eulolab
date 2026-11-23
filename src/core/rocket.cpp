#include "rocket.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

namespace eulo {
namespace {
constexpr double pi = 3.14159265358979323846;
// Inspector section offsets; sum = 52 editable model fields. These origins
// describe *inputs*, never a physical measurement or a validated model.
constexpr int inputOffsets[]={0,6,11,20,22,28,32,39,47,52};
struct Part { double mass, x, intrinsicI; };
struct ScalarField { const char* key; double Rocket::*member; };
constexpr ScalarField scalars[] = {
    {"bodyLength",&Rocket::bodyLength},{"radius",&Rocket::radius},
    {"wall",&Rocket::wall},{"bodyDensity",&Rocket::bodyDensity},
    {"airframeCd",&Rocket::airframeCd},{"noseLength",&Rocket::noseLength},
    {"noseWall",&Rocket::noseWall},{"noseDensity",&Rocket::noseDensity},
    {"finSpan",&Rocket::finSpan},{"finRootChord",&Rocket::finRootChord},
    {"finTipChord",&Rocket::finTipChord},{"finSweep",&Rocket::finSweep},
    {"finThickness",&Rocket::finThickness},{"finSetback",&Rocket::finSetback},
    {"finDensity",&Rocket::finDensity},{"electronicsMass",&Rocket::electronicsMass},
    {"electronicsFromTail",&Rocket::electronicsFromTail},
    {"motorFromTail",&Rocket::motorFromTail},{"mountLength",&Rocket::mountLength},
    {"mountBoreRadius",&Rocket::mountBoreRadius},{"mountWall",&Rocket::mountWall},
    {"mountDensity",&Rocket::mountDensity},
    {"parachuteDiameter",&Rocket::parachuteDiameter},{"canopyCd",&Rocket::canopyCd},
    {"canopyArealDensity",&Rocket::canopyArealDensity},
    {"recoveryFromTail",&Rocket::recoveryFromTail},
    {"insertLength",&Rocket::insertLength},
    {"insertOuterRadius",&Rocket::insertOuterRadius},
    {"insertBoreRadius",&Rocket::insertBoreRadius},
    {"insertFromTail",&Rocket::insertFromTail},
    {"insertDensity",&Rocket::insertDensity},
    {"launchTiltDeg",&Rocket::launchTiltDeg},
    {"launchHeadingDeg",&Rocket::launchHeadingDeg},
    {"windEast",&Rocket::windEast},{"windNorth",&Rocket::windNorth},
    {"gimbalPitchDeg",&Rocket::gimbalPitchDeg},
    {"gimbalYawDeg",&Rocket::gimbalYawDeg},
    {"cpFromTail",&Rocket::cpFromTail},
    {"angularDamping",&Rocket::angularDamping},
    {"referenceUnitMm",&Rocket::referenceUnitMm},
    {"referenceEastMm",&Rocket::referenceEastMm},
    {"referenceTailMm",&Rocket::referenceTailMm},
    {"referenceNorthMm",&Rocket::referenceNorthMm}
};
constexpr const char* integers[] = {
    "material","noseProfile","noseMaterial","finMaterial","finCount",
    "motorIndex","insertEnabled","insertMaterial","referenceFormat"
};

bool readEul(FILE* file,Rocket& candidate,int schema) {
    bool seen[std::size(scalars)+std::size(integers)] = {};
    bool seenOrigins=false;
    int values[std::size(integers)] = {};
    char line[256];
    while (std::fgets(line,sizeof(line),file)) {
        const size_t length=std::strlen(line);
        if(length==0 || (line[length-1]!='\n' && !std::feof(file)))return false;
        while(size_t n=std::strlen(line)) {
            if(line[n-1]=='\n' || line[n-1]=='\r')line[n-1]='\0';
            else break;
        }
        char* equal=std::strchr(line,'=');
        if(!equal || equal==line || std::strchr(equal+1,'='))return false;
        *equal='\0';
        const char* text=equal+1;
        if(std::strcmp(line,"inputOrigins")==0) {
            if(schema!=3 || seenOrigins || std::strlen(text)!=Rocket::inputCount)
                return false;
            for(int i=0;i<Rocket::inputCount;++i) {
                if(text[i]<'0' || text[i]>'2')return false;
                candidate.inputOrigins[static_cast<size_t>(i)]=
                    static_cast<InputOrigin>(text[i]-'0');
            }
            seenOrigins=true;
            continue;
        }
        bool found=false;
        for(size_t i=0;i<std::size(scalars);++i) {
            if(std::strcmp(line,scalars[i].key)!=0)continue;
            if(seen[i])return false;
            char* end=nullptr;errno=0;
            const double number=std::strtod(text,&end);
            if(text==end || *end || errno==ERANGE || !std::isfinite(number))return false;
            candidate.*(scalars[i].member)=number;
            seen[i]=true;found=true;break;
        }
        if(found)continue;
        for(size_t i=0;i<std::size(integers);++i) {
            if(std::strcmp(line,integers[i])!=0)continue;
            if(seen[i+std::size(scalars)])return false;
            char* end=nullptr;errno=0;
            const long number=std::strtol(text,&end,10);
            if(text==end || *end || errno==ERANGE || number<INT_MIN || number>INT_MAX)return false;
            values[i]=static_cast<int>(number);
            seen[i+std::size(scalars)]=true;found=true;break;
        }
        if(!found)return false;
    }
    if(std::ferror(file) || (schema==3 && !seenOrigins))return false;
    for(size_t i=0;i<std::size(scalars);++i)
        if(!seen[i] && (schema>=2 || i<std::size(scalars)-4))return false;
    for(size_t i=0;i<std::size(integers);++i)
        if(!seen[std::size(scalars)+i] && (schema>=2 || i<std::size(integers)-1))return false;
    if(values[6]!=0 && values[6]!=1)return false;
    candidate.material=static_cast<Material>(values[0]);
    candidate.noseProfile=static_cast<NoseProfile>(values[1]);
    candidate.noseMaterial=static_cast<Material>(values[2]);
    candidate.finMaterial=static_cast<FinMaterial>(values[3]);
    candidate.finCount=values[4];candidate.motorIndex=values[5];
    candidate.insertEnabled=values[6]!=0;
    candidate.insertMaterial=static_cast<Material>(values[7]);
    candidate.referenceFormat=values[8];
    return true;
}
}

const char* materialName(Material material) {
    switch (material) {
        case Material::PVC: return "PVC";
        case Material::Aluminium: return "Aluminium";
        case Material::CarbonLaminate: return "Carbon laminate";
    }
    return "Unknown";
}
const char* finMaterialName(FinMaterial material) {
    switch (material) {
        case FinMaterial::Balsa: return "Balsa";
        case FinMaterial::Plywood: return "Plywood";
        case FinMaterial::CarbonLaminate: return "Carbon laminate";
    }
    return "Unknown";
}
const char* noseProfileName(NoseProfile profile) {
    return profile == NoseProfile::Cone ? "Conical" :
           profile == NoseProfile::TangentOgive ? "Tangent ogive" : "Unknown";
}
double illustrativeDensity(Material material) {
    switch (material) {
        case Material::PVC: return 1280.0;
        case Material::Aluminium: return 2700.0;
        case Material::CarbonLaminate: return 1500.0;
    }
    return 0.0;
}
double illustrativeFinDensity(FinMaterial material) {
    switch (material) {
        case FinMaterial::Balsa: return 210.0;
        case FinMaterial::Plywood: return 600.0;
        case FinMaterial::CarbonLaminate: return 1500.0;
    }
    return 0.0;
}

InputOrigin Rocket::origin(int part,int field) const {
    if(part<0 || part>=static_cast<int>(std::size(inputOffsets))-1 ||
       field<0 || field>=inputOffsets[part+1]-inputOffsets[part])
        return InputOrigin::LegacyUnknown;
    return inputOrigins[static_cast<size_t>(inputOffsets[part]+field)];
}
bool Rocket::markOrigin(int part,int field,InputOrigin source) {
    if(part<0 || part>=static_cast<int>(std::size(inputOffsets))-1 ||
       field<0 || field>=inputOffsets[part+1]-inputOffsets[part] ||
       static_cast<int>(source)>2)return false;
    inputOrigins[static_cast<size_t>(inputOffsets[part]+field)]=source;
    return true;
}

bool Rocket::valid() const {
    for(InputOrigin source:inputOrigins)
        if(static_cast<int>(source)>2)return false;
    return std::isfinite(bodyLength) && bodyLength >= 0.2 && bodyLength <= 3.0 &&
           std::isfinite(radius) && radius >= 0.01 && radius <= 0.1 &&
           std::isfinite(wall) && wall >= 0.0004 && wall < radius * 0.5 &&
           static_cast<int>(material) >= 0 && static_cast<int>(material) <= 2 &&
           std::isfinite(bodyDensity) && bodyDensity >= 80 && bodyDensity <= 8000 &&
           std::isfinite(airframeCd) && airframeCd >= 0.1 && airframeCd <= 2.0 &&
           std::isfinite(noseLength) && noseLength > radius && noseLength < bodyLength &&
           std::isfinite(noseWall) && noseWall >= 0.0004 && noseWall < radius * 0.5 &&
           static_cast<int>(noseProfile) >= 0 && static_cast<int>(noseProfile) <= 1 &&
           static_cast<int>(noseMaterial) >= 0 && static_cast<int>(noseMaterial) <= 2 &&
           std::isfinite(noseDensity) && noseDensity >= 80 && noseDensity <= 8000 &&
           finCount >= 2 && finCount <= 8 &&
           std::isfinite(finSpan) && finSpan >= 0.01 && finSpan <= 0.25 &&
           std::isfinite(finRootChord) && finRootChord >= 0.02 &&
           std::isfinite(finTipChord) && finTipChord >= 0.01 &&
           std::isfinite(finSweep) && finSweep >= 0 && finSweep <= 0.25 &&
           std::isfinite(finThickness) && finThickness >= 0.001 && finThickness <= 0.012 &&
           std::isfinite(finSetback) && finSetback >= 0 &&
           finSetback + finRootChord <= bodyLength &&
           finSetback + finSweep + finTipChord <= bodyLength &&
           static_cast<int>(finMaterial) >= 0 && static_cast<int>(finMaterial) <= 2 &&
           std::isfinite(finDensity) && finDensity >= 80 && finDensity <= 8000 &&
           std::isfinite(electronicsMass) && electronicsMass >= 0 && electronicsMass < 2 &&
           std::isfinite(electronicsFromTail) && electronicsFromTail >= 0 &&
           electronicsFromTail <= bodyLength &&
           motorIndex >= 0 && motorIndex <= 1 &&
           std::isfinite(motorFromTail) && motorFromTail >= 0.005 &&
           motorFromTail <= bodyLength &&
           std::isfinite(mountLength) && mountLength >= 0.05 &&
           mountLength <= 0.4 && mountLength <= bodyLength &&
           std::isfinite(mountBoreRadius) && mountBoreRadius >= 0.006 &&
           mountBoreRadius <= 0.025 &&
           std::isfinite(mountWall) && mountWall >= 0.0004 &&
           mountWall <= 0.005 && mountBoreRadius+mountWall <= radius-wall &&
           std::isfinite(mountDensity) && mountDensity >= 80 && mountDensity <= 8000 &&
           std::isfinite(parachuteDiameter) && (parachuteDiameter == 0 ||
               (parachuteDiameter >= 0.1 && parachuteDiameter <= 1.5)) &&
           std::isfinite(canopyCd) && canopyCd >= 0.2 && canopyCd <= 3.0 &&
           std::isfinite(canopyArealDensity) && canopyArealDensity >= 0.01 &&
           canopyArealDensity <= 1.0 &&
           std::isfinite(recoveryFromTail) && recoveryFromTail >= 0 &&
           recoveryFromTail <= bodyLength &&
           std::isfinite(insertLength) && insertLength >= 0.002 && insertLength <= 0.5 &&
           std::isfinite(insertOuterRadius) && insertOuterRadius >= 0.001 &&
           insertOuterRadius <= 0.04 &&
           std::isfinite(insertBoreRadius) && insertBoreRadius >= 0 &&
           (insertBoreRadius == 0 || insertOuterRadius-insertBoreRadius >= 0.0004) &&
           static_cast<int>(insertMaterial) >= 0 && static_cast<int>(insertMaterial) <= 2 &&
           std::isfinite(insertDensity) && insertDensity >= 80 && insertDensity <= 8000 &&
           std::isfinite(insertFromTail) && insertFromTail >= 0 &&
           insertFromTail <= bodyLength &&
           (!insertEnabled || (insertOuterRadius <= radius-wall &&
               insertFromTail-insertLength*0.5 >= mountLength &&
               insertFromTail+insertLength*0.5 <= bodyLength)) &&
           std::isfinite(launchTiltDeg) && launchTiltDeg >= 0 && launchTiltDeg <= 20 &&
           std::isfinite(launchHeadingDeg) && launchHeadingDeg >= -180 &&
           launchHeadingDeg <= 180 &&
           std::isfinite(windEast) && std::fabs(windEast) <= 30 &&
           std::isfinite(windNorth) && std::fabs(windNorth) <= 30 &&
           std::isfinite(gimbalPitchDeg) && std::fabs(gimbalPitchDeg) <= 5 &&
           std::isfinite(gimbalYawDeg) && std::fabs(gimbalYawDeg) <= 5 &&
           std::isfinite(cpFromTail) && cpFromTail >= 0 && cpFromTail <= bodyLength &&
           std::isfinite(angularDamping) && angularDamping >= 0 &&
           angularDamping <= 0.1 && referenceFormat >= 0 && referenceFormat <= 2 &&
           std::isfinite(referenceUnitMm) && referenceUnitMm >= 0.01 &&
           referenceUnitMm <= 1000 &&
           std::isfinite(referenceEastMm) && std::fabs(referenceEastMm) <= 3000 &&
           std::isfinite(referenceTailMm) && std::fabs(referenceTailMm) <= 3000 &&
           std::isfinite(referenceNorthMm) && std::fabs(referenceNorthMm) <= 3000;
}

double noseRadius(const Rocket& rocket, double fraction) {
    const double t = std::clamp(fraction, 0.0, 1.0);
    if (rocket.noseProfile == NoseProfile::Cone) return rocket.radius * (1.0-t);
    const double L = rocket.noseLength, R = rocket.radius;
    const double rho = (R*R + L*L) / (2.0*R);
    const double x = L * (1.0-t); // distance from tip
    return std::clamp(std::sqrt(std::max(0.0, rho*rho-(L-x)*(L-x))) + R-rho, 0.0, R);
}

MassProperties massProperties(const Rocket& r, double motorMass, double propellantMass) {
    MassProperties result;
    if (!r.valid() || !std::isfinite(motorMass) || !std::isfinite(propellantMass) ||
        motorMass < 0 || propellantMass < 0 || propellantMass > motorMass) return result;
    const double inner = r.radius - r.wall;
    const double bodyMass = pi * (r.radius*r.radius - inner*inner) * r.bodyLength*r.bodyDensity;
    // The chosen nose profile changes both its shaded mesh and integrated shell mass.
    double noseMass = 0, noseMoment = 0;
    for (int i=0; i<24; ++i) {
        const double t0 = i/24.0, t1 = (i+1)/24.0;
        const double r0 = noseRadius(r,t0), r1 = noseRadius(r,t1);
        const double band = pi*(r0+r1)*std::hypot(r1-r0,r.noseLength/24.0);
        const double mass = band*r.noseWall*r.noseDensity;
        noseMass += mass;
        noseMoment += mass*(r.bodyLength + r.noseLength*(t0+t1)/2.0);
    }
    const double finArea = (r.finRootChord+r.finTipChord)*r.finSpan/2.0;
    const double finMass = r.finCount*finArea*r.finThickness*r.finDensity;
    const double centroidSpanFraction =
        (r.finRootChord/2.0 + (r.finTipChord-r.finRootChord)/3.0) /
        ((r.finRootChord+r.finTipChord)/2.0);
    const double finCg = r.finSetback + r.finRootChord/2.0 +
        centroidSpanFraction*(r.finSweep + (r.finTipChord-r.finRootChord)/2.0);
    const double mountOuter=r.mountBoreRadius+r.mountWall;
    const double mountMass=pi*(mountOuter*mountOuter-r.mountBoreRadius*r.mountBoreRadius)*
        r.mountLength*r.mountDensity;
    const double canopyMass = r.parachuteDiameter > 0 ?
        pi*r.parachuteDiameter*r.parachuteDiameter/4.0*r.canopyArealDensity + 0.002 : 0.0;
    const double insertMass = r.insertEnabled ? pi*(r.insertOuterRadius*r.insertOuterRadius-
        r.insertBoreRadius*r.insertBoreRadius)*r.insertLength*r.insertDensity : 0.0;
    const Part parts[] = {
        {bodyMass, r.bodyLength*0.5, bodyMass*r.bodyLength*r.bodyLength/12.0},
        {mountMass,r.mountLength*0.5,mountMass*r.mountLength*r.mountLength/12.0},
        {noseMass, noseMass>0 ? noseMoment/noseMass : r.bodyLength,
            noseMass*r.noseLength*r.noseLength/18.0},
        {finMass, finCg, finMass*r.finRootChord*r.finRootChord/12.0},
        {r.electronicsMass, r.electronicsFromTail, 0.0},
        {canopyMass, r.recoveryFromTail, 0.0},
        {motorMass, r.motorFromTail, motorMass*0.07*0.07/12.0},
        {insertMass, r.insertFromTail, insertMass*(r.insertLength*r.insertLength+
            3*(r.insertOuterRadius*r.insertOuterRadius+
               r.insertBoreRadius*r.insertBoreRadius))/12.0}
    };
    double weighted = 0;
    for (const Part& part : parts) {
        result.loaded += part.mass;
        weighted += part.mass*part.x;
    }
    result.dry = result.loaded - propellantMass;
    if (result.loaded > 0) {
        result.cgFromTail = weighted/result.loaded;
        for (const Part& part : parts) {
            const double dx = part.x - result.cgFromTail;
            result.momentPitch += part.intrinsicI + part.mass*dx*dx;
        }
        result.momentRoll = bodyMass*(r.radius*r.radius+inner*inner)/2.0 +
            mountMass*(mountOuter*mountOuter+r.mountBoreRadius*r.mountBoreRadius)/2.0 +
            noseMass*r.radius*r.radius/4.0 +
            finMass*std::pow(r.radius+r.finSpan*0.5,2) +
            (r.electronicsMass+canopyMass+motorMass)*r.radius*r.radius/4.0 +
            insertMass*(r.insertOuterRadius*r.insertOuterRadius+
                        r.insertBoreRadius*r.insertBoreRadius)/2.0;
    }
    return result;
}

bool saveProject(const Rocket& r, const char* path) {
    if (!r.valid() || !path) return false;
    const std::string temporary = std::string(path) + ".tmp";
    FILE* f = std::fopen(temporary.c_str(),"wb");
    if (!f) return false;
    bool ok=std::fputs("EULOLAB/3\n",f)>=0;
    for(const ScalarField& field:scalars)
        if(ok)ok=std::fprintf(f,"%s=%.17g\n",field.key,r.*(field.member))>0;
    const int values[]={static_cast<int>(r.material),static_cast<int>(r.noseProfile),
        static_cast<int>(r.noseMaterial),static_cast<int>(r.finMaterial),r.finCount,
        r.motorIndex,r.insertEnabled?1:0,static_cast<int>(r.insertMaterial),
        r.referenceFormat};
    static_assert(std::size(values)==std::size(integers));
    for(size_t i=0;i<std::size(integers);++i)
        if(ok)ok=std::fprintf(f,"%s=%d\n",integers[i],values[i])>0;
    if(ok)ok=std::fputs("inputOrigins=",f)>=0;
    for(InputOrigin source:r.inputOrigins)
        if(ok)ok=std::fputc('0'+static_cast<int>(source),f)!=EOF;
    if(ok)ok=std::fputc('\n',f)!=EOF;
    const bool closed = std::fclose(f) == 0;
    if (!ok || !closed || std::rename(temporary.c_str(), path) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

bool loadProject(Rocket& rocket, const char* path) {
    if (!path) return false;
    FILE* f = std::fopen(path,"rb");
    if (!f) return false;
    char header[32] = {};
    Rocket candidate;
    int material=-1, profile=-1, noseMaterial=-1, finMaterial=-1;
    bool ok = std::fgets(header,sizeof(header),f) != nullptr;
    if (ok && (std::strcmp(header,"EULOLAB/1\n")==0 ||
               std::strcmp(header,"EULOLAB/2\n")==0 ||
               std::strcmp(header,"EULOLAB/3\n")==0)) {
        const int schema=header[8]-'0'; // EULOLAB/x
        ok=readEul(f,candidate,schema);
        material=static_cast<int>(candidate.material);
    } else if (ok && std::strcmp(header,"EULOROCKET3\n")==0) {
        double* values[] = {
            &candidate.bodyLength,&candidate.radius,&candidate.wall,&candidate.bodyDensity,&candidate.airframeCd,
            &candidate.noseLength,&candidate.noseWall,&candidate.noseDensity,
            &candidate.finSpan,&candidate.finRootChord,&candidate.finTipChord,&candidate.finSweep,
            &candidate.finThickness,&candidate.finSetback,&candidate.finDensity,
            &candidate.electronicsMass,&candidate.electronicsFromTail,&candidate.motorFromTail,
            &candidate.mountLength,&candidate.mountBoreRadius,&candidate.mountWall,
            &candidate.mountDensity,&candidate.parachuteDiameter,&candidate.canopyCd,
            &candidate.canopyArealDensity,&candidate.recoveryFromTail
        };
        for (double* value : values) if (ok) ok = std::fscanf(f,"%lf",value) == 1;
        if (ok) ok = std::fscanf(f,"%d %d %d %d %d %d",&material,&profile,
            &noseMaterial,&finMaterial,&candidate.finCount,&candidate.motorIndex) == 6;
        candidate.noseProfile = static_cast<NoseProfile>(profile);
        candidate.noseMaterial = static_cast<Material>(noseMaterial);
        candidate.finMaterial = static_cast<FinMaterial>(finMaterial);
    } else if (ok && std::strcmp(header,"EULOROCKET2\n")==0) {
        ok = std::fscanf(f,"%lf %lf %lf %lf %lf %lf %lf %lf %d %d",
            &candidate.bodyLength,&candidate.radius,&candidate.wall,&candidate.noseLength,
            &candidate.finSpan,&candidate.electronicsMass,&candidate.bodyDensity,
            &candidate.parachuteDiameter,&material,&candidate.motorIndex) == 10;
        candidate.noseWall = candidate.wall;
        candidate.noseDensity = candidate.bodyDensity;
        candidate.noseMaterial = static_cast<Material>(material);
    } else if (ok && std::strcmp(header,"EULOROCKET1\n")==0) {
        candidate.parachuteDiameter=0;
        ok = std::fscanf(f,"%lf %lf %lf %lf %lf %lf %lf %d",
            &candidate.bodyLength,&candidate.radius,&candidate.wall,&candidate.noseLength,
            &candidate.finSpan,&candidate.electronicsMass,&candidate.bodyDensity,&material) == 8;
        candidate.noseWall = candidate.wall;
        candidate.noseDensity = candidate.bodyDensity;
        candidate.noseMaterial = static_cast<Material>(material);
    } else ok = false;
    if (ok && (std::strcmp(header,"EULOROCKET1\n")==0 ||
               std::strcmp(header,"EULOROCKET2\n")==0)) {
        candidate.electronicsFromTail = candidate.bodyLength*0.66;
        candidate.recoveryFromTail = candidate.bodyLength*0.80;
        candidate.finRootChord = std::min(candidate.finRootChord,candidate.bodyLength*0.45);
        candidate.finTipChord = std::min(candidate.finTipChord,candidate.bodyLength*0.30);
    }
    candidate.material = static_cast<Material>(material);
    if(std::strcmp(header,"EULOLAB/3\n")!=0)
        candidate.inputOrigins.fill(InputOrigin::LegacyUnknown);
    const int trailing = ok ? std::fgetc(f) : EOF;
    // Whitespace after the final value is allowed; further data is not.
    if (ok && trailing != EOF) {
        if (trailing != '\n' && trailing != '\r' && trailing != ' ' && trailing != '\t') ok = false;
        int ch;
        while (ok && (ch=std::fgetc(f)) != EOF)
            if (ch != '\n' && ch != '\r' && ch != ' ' && ch != '\t') ok = false;
    }
    std::fclose(f);
    if (!ok || !candidate.valid()) return false;
    rocket = candidate;
    return true;
}

} // namespace eulo
