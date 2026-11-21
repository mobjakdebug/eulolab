#include "core/rocket.hpp"
#include "core/motor.hpp"
#include "core/flight.hpp"
#include "core/telemetry.hpp"
#include "core/mesh.hpp"
#include "core/bench_wire.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
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
static void testBenchWire() {
    using namespace eulo::bench;
    const uint8_t known[]={'1','2','3','4','5','6','7','8','9'};
    assert(crc16(known,sizeof(known))==0x29b1);
    Frame request;
    request.kind=Kind::Pulse;request.sequence=1234;request.length=5;
    request.payload[0]=0;write16(request.payload+1,static_cast<uint16_t>(-200));
    write16(request.payload+3,250);
    uint8_t bytes[maxFrame]={};
    const size_t length=encode(request,bytes);
    assert(length==14 && bytes[0]=='E' && bytes[1]=='U' &&
           bytes[2]==1 && read16(bytes+4)==1234);
    Decoder decoder;
    Frame received;
    assert(!decoder.push(0x55,received));
    assert(!decoder.push('E',received));
    assert(!decoder.push('E',received)); // new sync after an abandoned lead byte
    bool decoded=false;
    for(size_t i=1;i<length;++i)decoded=decoder.push(bytes[i],received) || decoded;
    assert(decoded && received.kind==Kind::Pulse && received.sequence==1234 &&
           received.length==5 &&
           static_cast<int16_t>(read16(received.payload+1))==-200 &&
           read16(received.payload+3)==250);
    bytes[8]^=0x01;
    decoder.reset();decoded=false;
    for(size_t i=0;i<length;++i)decoded=decoder.push(bytes[i],received) || decoded;
    assert(!decoded);
    assert(safePulse(0,-300,300) && safePulse(3,300,50));
    for(int angle:{-301,0,301})assert(!safePulse(0,angle,100));
    assert(!safePulse(4,100,100) && !safePulse(0,100,0) &&
           !safePulse(0,100,1000));
    Frame overflow;overflow.length=maxPayload+1;
    assert(encode(overflow,bytes)==0);
    std::puts("PASS bench wire: CRC, fragmentation, resync, corruption and hard pulse bounds");
}
static void testMeshes() {
    Rocket r;
    r.insertEnabled=true;
    assert(r.valid());
    const auto visual=visualMesh(r);
    assert(visual.size()>900 && visual.back().surface==Surface::Insert);
    assert(exportVisualObj(r,"out/assembly-test.obj"));
    ReferenceMesh mesh;
    std::string error;
    assert(mesh.load("out/assembly-test.obj",error));
    assert(mesh.triangles.size()==visual.size() &&
           near(mesh.lower.y,0,1e-4) &&
           near(mesh.upper.y,(r.bodyLength+r.noseLength)*1000,1e-3));
    const size_t before=mesh.triangles.size();
    FILE* file=std::fopen("out/reference-test.obj","wb");
    assert(file && std::fputs("# supported OBJ face variants\n"
        "v 0 0 0\nv 10 0 0\nv 10 10 0\nv 0 10 0\n"
        "vt 0 0\nusemtl ignored\nf -4/1 -3/1 -2/1 -1/1\n",file)>0 &&
        std::fclose(file)==0);
    assert(mesh.load("out/reference-test.obj",error) && mesh.triangles.size()==2 &&
           near(mesh.upper.x,10));
    file=std::fopen("out/reference-test.obj","wb");
    assert(file && std::fputs("v 0 0 0\nv 1 0 0\nf 1 2 3\n",file)>0 &&
           std::fclose(file)==0);
    assert(!mesh.load("out/reference-test.obj",error) &&
           mesh.triangles.size()==2 && !error.empty());
    file=std::fopen("out/reference-test.stl","wb");
    assert(file && std::fputs("solid fixture\n"
        "facet normal 0 0 1\nouter loop\nvertex 0 0 0\n"
        "vertex 10 0 0\nvertex 0 10 0\nendloop\nendfacet\nendsolid fixture\n",file)>0 &&
        std::fclose(file)==0);
    assert(mesh.load("out/reference-test.stl",error) && mesh.triangles.size()==1 &&
           near(mesh.upper.y,10));
    unsigned char binary[134]={};
    binary[80]=1;
    const float vertices[12]={0,0,1, 0,0,0, 5,0,0, 0,8,0};
    for(size_t i=0;i<12;++i) {
        uint32_t bits=0;
        std::memcpy(&bits,&vertices[i],sizeof(bits));
        for(int byte=0;byte<4;++byte)
            binary[84+i*4+byte]=static_cast<unsigned char>(bits>>(8*byte));
    }
    file=std::fopen("out/reference-test.stl","wb");
    assert(file && std::fwrite(binary,1,sizeof(binary),file)==sizeof(binary) &&
           std::fclose(file)==0);
    assert(mesh.load("out/reference-test.stl",error) && mesh.triangles.size()==1 &&
           near(mesh.upper.x,5) && near(mesh.upper.y,8));
    std::remove("out/assembly-test.obj");
    std::remove("out/reference-test.obj");
    std::remove("out/reference-test.stl");
    assert(before>800);
    std::puts("PASS meshes: visual OBJ export/round-trip, OBJ polygon, ASCII/binary STL, reject invalid");
}
static void testTelemetry() {
    const char* path="out/telemetry-test.csv";
    FILE* file=std::fopen(path,"wb");
    assert(file && std::fputs("time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s\n",file)>0);
    for(int i=0;i<=40;++i) {
        const double altitude=100+0.03*std::sin(i)+(i>20?(i-20)*0.2:0);
        const double gy=-.1+(i>20?20:0);
        assert(std::fprintf(file,"%.2f,%.5f,0.15,%.2f,0.05\n",i*.1,altitude,gy)>0);
    }
    assert(std::fclose(file)==0);
    TelemetryRecording log;
    std::string error;
    assert(log.load(path,error) && log.samples.size()==41 && !log.calibrated);
    assert(near(log.at(3.5).attitude.w,1));
    assert(log.calibrate(error) && log.calibrated);
    assert(near(log.gyroBias.x,.15,1e-8) && near(log.gyroBias.y,-.1,1e-8) &&
           near(log.altitudeZero,100,.05));
    const auto mid=log.at(3.55);
    assert(mid.altitude-log.altitudeZero>2 && near(mid.gyro.y-log.gyroBias.y,20));
    const auto last=log.at(4);
    assert(last.attitude.y>.29 && last.attitude.y<.38 &&
           near(last.attitude.w*last.attitude.w+last.attitude.x*last.attitude.x+
                last.attitude.y*last.attitude.y+last.attitude.z*last.attitude.z,1,1e-8));
    assert(log.at(-1).time==0 && near(log.at(999).time,4));
    const auto size=log.samples.size();
    file=std::fopen(path,"wb");
    assert(file && std::fputs("time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s\n"
        "0,100,0,0,0\n0.1,nan,0,0,0\n",file)>0 && std::fclose(file)==0);
    assert(!log.load(path,error) && !error.empty() && log.samples.size()==size && log.calibrated);
    file=std::fopen(path,"wb");
    assert(file && std::fputs("time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s\n"
        "0,100,0,0,0\n0.1,100,5,0,0\n",file)>0 && std::fclose(file)==0);
    TelemetryRecording moving;
    assert(moving.load(path,error) && !moving.calibrate(error) && !error.empty());
    file=std::fopen(path,"wb");
    assert(file && std::fputs("time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s\n"
        "0,100,0,0,0\n0,100,0,0,0\n",file)>0 && std::fclose(file)==0);
    assert(!moving.load(path,error)); // timestamps must increase
    std::remove(path);
    std::puts("PASS telemetry: CSV bounds, stationary calibration, gyro integration, transactional failure");
}
int main() {
    testBenchWire();
    testMeshes();
    testTelemetry();
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
    assert(mass.loaded > mass.dry && mass.cgFromTail > 0 &&
           mass.momentPitch > 0 && mass.momentRoll > 0);
    rocket.material = Material::Aluminium;
    rocket.bodyDensity = illustrativeDensity(rocket.material);
    assert(massProperties(rocket, .0205, .0096).loaded > mass.loaded);
    rocket=Rocket{};
    const double baselineMass=massProperties(rocket,.0205,.0096).loaded;
    rocket.mountLength=.10;
    assert(rocket.valid() && massProperties(rocket,.0205,.0096).loaded>baselineMass);
    rocket=Rocket{};
    const double coneMass=massProperties(rocket,.0205,.0096).loaded;
    rocket.noseProfile=NoseProfile::TangentOgive;
    assert(near(noseRadius(rocket,0),rocket.radius) &&
           near(noseRadius(rocket,1),0,1e-7) &&
           !near(noseRadius(rocket,.5),rocket.radius*.5));
    assert(!near(massProperties(rocket,.0205,.0096).loaded,coneMass));
    rocket.finCount=4;
    assert(massProperties(rocket,.0205,.0096).loaded>coneMass);
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
        Rocket expected;
        expected.bodyLength=.38;expected.radius=.014;expected.wall=.001;
        expected.material=Material::Aluminium;expected.bodyDensity=2700;
        expected.airframeCd=.8;
        expected.noseLength=.12;expected.noseWall=.0011;
        expected.noseProfile=NoseProfile::TangentOgive;
        expected.noseMaterial=Material::CarbonLaminate;expected.noseDensity=1500;
        expected.finCount=4;expected.finSpan=.056;expected.finRootChord=.100;
        expected.finTipChord=.060;expected.finSweep=.020;
        expected.finThickness=.003;expected.finSetback=.008;
        expected.finMaterial=FinMaterial::Plywood;expected.finDensity=600;
        expected.electronicsMass=.025;expected.electronicsFromTail=.260;
        expected.motorIndex=1;expected.motorFromTail=.038;
        expected.mountLength=.09;expected.mountBoreRadius=.0098;
        expected.mountWall=.001;expected.mountDensity=1100;
        expected.parachuteDiameter=.36;expected.canopyCd=1.3;
        expected.canopyArealDensity=.11;expected.recoveryFromTail=.29;
        expected.insertEnabled=true;expected.insertLength=.045;
        expected.insertOuterRadius=.006;expected.insertBoreRadius=.002;
        expected.insertFromTail=.20;expected.insertMaterial=Material::Aluminium;
        expected.insertDensity=2700;
        expected.launchTiltDeg=4;expected.launchHeadingDeg=75;
        expected.windEast=-2.5;expected.windNorth=3.0;
        expected.gimbalPitchDeg=.3;expected.gimbalYawDeg=-.4;
        expected.cpFromTail=.12;expected.angularDamping=.005;
        expected.referenceFormat=2;expected.referenceUnitMm=2.5;
        expected.referenceEastMm=12;expected.referenceTailMm=18;
        expected.referenceNorthMm=-9;
        assert(expected.valid() && saveProject(expected,"out/roundtrip.eul"));
        FILE* text=std::fopen("out/roundtrip.eul","rb");
        char magic[32]={};
        assert(text && std::fgets(magic,sizeof(magic),text) &&
               std::string(magic)=="EULOLAB/2\n" && std::fclose(text)==0);
        Rocket actual;
        assert(loadProject(actual,"out/roundtrip.eul"));
        const double lhs[]={expected.bodyLength,expected.radius,expected.wall,
            expected.bodyDensity,expected.airframeCd,expected.noseLength,
            expected.noseWall,expected.noseDensity,expected.finSpan,
            expected.finRootChord,expected.finTipChord,expected.finSweep,
            expected.finThickness,expected.finSetback,expected.finDensity,
            expected.electronicsMass,expected.electronicsFromTail,
            expected.motorFromTail,expected.mountLength,expected.mountBoreRadius,
            expected.mountWall,expected.mountDensity,expected.parachuteDiameter,
            expected.canopyCd,expected.canopyArealDensity,expected.recoveryFromTail};
        const double rhs[]={actual.bodyLength,actual.radius,actual.wall,
            actual.bodyDensity,actual.airframeCd,actual.noseLength,
            actual.noseWall,actual.noseDensity,actual.finSpan,
            actual.finRootChord,actual.finTipChord,actual.finSweep,
            actual.finThickness,actual.finSetback,actual.finDensity,
            actual.electronicsMass,actual.electronicsFromTail,
            actual.motorFromTail,actual.mountLength,actual.mountBoreRadius,
            actual.mountWall,actual.mountDensity,actual.parachuteDiameter,
            actual.canopyCd,actual.canopyArealDensity,actual.recoveryFromTail};
        for(size_t i=0;i<sizeof(lhs)/sizeof(lhs[0]);++i)assert(near(lhs[i],rhs[i]));
        assert(actual.material==expected.material && actual.noseMaterial==expected.noseMaterial &&
               actual.finMaterial==expected.finMaterial && actual.noseProfile==expected.noseProfile &&
               actual.finCount==4 && actual.motorIndex==1);
        assert(actual.insertEnabled && actual.insertMaterial==Material::Aluminium &&
               near(actual.insertLength,expected.insertLength) &&
               near(actual.insertOuterRadius,expected.insertOuterRadius) &&
               near(actual.insertBoreRadius,expected.insertBoreRadius) &&
               near(actual.insertFromTail,expected.insertFromTail) &&
               near(actual.insertDensity,expected.insertDensity));
        assert(near(actual.launchTiltDeg,expected.launchTiltDeg) &&
               near(actual.launchHeadingDeg,expected.launchHeadingDeg) &&
               near(actual.windEast,expected.windEast) &&
               near(actual.windNorth,expected.windNorth) &&
               near(actual.gimbalPitchDeg,expected.gimbalPitchDeg) &&
               near(actual.gimbalYawDeg,expected.gimbalYawDeg) &&
               near(actual.cpFromTail,expected.cpFromTail) &&
               near(actual.angularDamping,expected.angularDamping));
        assert(actual.referenceFormat==2 && near(actual.referenceUnitMm,2.5) &&
               near(actual.referenceEastMm,12) && near(actual.referenceTailMm,18) &&
               near(actual.referenceNorthMm,-9));
        FILE* f=std::fopen("out/roundtrip.eul","ab");
        assert(f && std::fputs("windEast=99\n",f)>0 && std::fclose(f)==0);
        assert(!loadProject(actual,"out/roundtrip.eul") && actual.insertEnabled &&
               near(actual.windEast,-2.5)); // duplicate key does not replace loaded model
        std::remove("out/roundtrip.eul");
    }
    {
        Rocket source;
        source.launchTiltDeg=6;source.windEast=-3;
        assert(saveProject(source,"out/schema2.eul"));
        FILE* current=std::fopen("out/schema2.eul","rb");
        FILE* older=std::fopen("out/schema1.eul","wb");
        char line[256];
        assert(current && older && std::fgets(line,sizeof(line),current));
        assert(std::fputs("EULOLAB/1\n",older)>0);
        while(std::fgets(line,sizeof(line),current))
            if(std::strncmp(line,"reference",9)!=0)assert(std::fputs(line,older)>0);
        assert(std::fclose(current)==0 && std::fclose(older)==0);
        Rocket migrated;
        assert(loadProject(migrated,"out/schema1.eul") &&
               near(migrated.launchTiltDeg,6) && near(migrated.windEast,-3) &&
               migrated.referenceFormat==0 && near(migrated.referenceUnitMm,1));
        std::remove("out/schema2.eul");std::remove("out/schema1.eul");
    }
    {
        FILE* legacy2=std::fopen("out/legacy2.ini","wb");
        assert(legacy2);
        assert(std::fputs("EULOROCKET2\n0.55 0.028 0.002 0.16 0.085 0.08 1280 0.36 0 1\n",legacy2)>0);
        assert(std::fclose(legacy2)==0);
        assert(loadProject(rocket,"out/legacy2.ini") && rocket.motorIndex==1 &&
               near(rocket.mountBoreRadius,.0095));
        std::remove("out/legacy2.ini");
    }
    {
        FILE* legacy = std::fopen("out/legacy.ini","wb");
        assert(legacy);
        assert(std::fputs("EULOROCKET1\n0.55 0.028 0.002 0.16 0.085 0.08 1280 0\n",legacy)>0);
        assert(std::fclose(legacy)==0);
        assert(loadProject(rocket,"out/legacy.ini") &&
               rocket.parachuteDiameter==0 && rocket.motorIndex==0);
        std::remove("out/legacy.ini");
    }
    {
        FILE* legacy3=std::fopen("out/legacy3.ini","wb");
        assert(legacy3);
        Rocket old;
        const double values[]={old.bodyLength,old.radius,old.wall,old.bodyDensity,
            old.airframeCd,old.noseLength,old.noseWall,old.noseDensity,
            old.finSpan,old.finRootChord,old.finTipChord,old.finSweep,
            old.finThickness,old.finSetback,old.finDensity,old.electronicsMass,
            old.electronicsFromTail,old.motorFromTail,old.mountLength,
            old.mountBoreRadius,old.mountWall,old.mountDensity,
            old.parachuteDiameter,old.canopyCd,old.canopyArealDensity,
            old.recoveryFromTail};
        assert(std::fputs("EULOROCKET3\n",legacy3)>0);
        for(double value:values)assert(std::fprintf(legacy3,"%.17g\n",value)>0);
        assert(std::fputs("0 0 0 0 3 0\n",legacy3)>0 && std::fclose(legacy3)==0);
        assert(loadProject(rocket,"out/legacy3.ini") && rocket.valid() &&
               !rocket.insertEnabled && near(rocket.launchTiltDeg,2));
        std::remove("out/legacy3.ini");
    }
    {
        Rocket insert;
        const auto empty=massProperties(insert,.0205,.0096);
        insert.insertEnabled=true;
        assert(insert.valid());
        const auto built=massProperties(insert,.0205,.0096);
        const double expected=3.14159265358979323846*insert.insertLength*
            (insert.insertOuterRadius*insert.insertOuterRadius-
             insert.insertBoreRadius*insert.insertBoreRadius)*insert.insertDensity;
        assert(near(built.loaded-empty.loaded,expected,1e-10) &&
               built.momentPitch>empty.momentPitch && built.momentRoll>empty.momentRoll);
        insert.insertBoreRadius=.002;
        assert(insert.valid() && massProperties(insert,.0205,.0096).loaded<built.loaded);
        insert.insertFromTail=.04;assert(!insert.valid()); // sleeve collision
        insert=Rocket{};insert.insertEnabled=true;insert.insertOuterRadius=.02;
        assert(!insert.valid()); // insert exceeds body's inner radius
        insert=Rocket{};insert.insertBoreRadius=insert.insertOuterRadius;
        assert(!insert.valid()); // hollow tube needs a wall
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
    const auto full=massProperties(rocket,c6.loadedKg,c6.propellantKg);
    const auto spent=massProperties(rocket,c6.loadedKg-c6.propellantKg,0);
    assert(spent.cgFromTail>full.cgFromTail && near(spent.dry,full.dry));
    assert(flight.valid && flight.apogee > 20 && flight.apogee < 3000);
    assert(flight.apogeeTime > c6.burnTime() && flight.maxG > 1 && flight.samples.size() > 40);
    assert(flight.landed && flight.maxQ > 0 && flight.touchdownSpeed > 0);
    assert(near(flight.at(-1).altitude, 0));
    assert(near(flight.at(flight.apogeeTime).altitude, flight.apogee, .5));
    assert(near(flight.at(1e6).altitude, 0));
    rocket.parachuteDiameter=0;
    auto ballistic=simulate(rocket,c6);
    assert(ballistic.valid && ballistic.landed && ballistic.touchdownSpeed>flight.touchdownSpeed*2);
    rocket=Rocket{};
    rocket.airframeCd=1.4;
    assert(simulate(rocket,c6).apogee < flight.apogee);
    rocket=Rocket{};
    rocket.canopyCd=.3;
    assert(simulate(rocket,c6).touchdownSpeed > flight.touchdownSpeed);
    rocket=Rocket{};
    assert(motorFits(rocket,c6) && motorFits(rocket,d9));
    rocket.mountBoreRadius=.0088;
    assert(rocket.valid() && !motorFits(rocket,c6) && !simulate(rocket,c6).valid);
    rocket=Rocket{};
    rocket.mountLength=.06;
    assert(rocket.valid() && !motorFits(rocket,c6));
    rocket=Rocket{};
    rocket.motorFromTail=.045;
    assert(rocket.valid() && !motorFits(rocket,c6));
    rocket=Rocket{};
    rocket.radius=.010; rocket.wall=.002;
    assert(!rocket.valid() && !simulate(rocket,c6).valid); // sleeve cannot fit tube
    rocket=Rocket{};
    rocket.windEast=rocket.windNorth=0;
    rocket.launchTiltDeg=0;
    const auto vertical=simulate(rocket,c6);
    assert(vertical.valid && vertical.landed && near(vertical.maxRange,0,1e-5));
    assert(near(vertical.at(1).position.z,0) &&
           near(vertical.at(1).position.y,vertical.at(1).altitude));
    assert(near(vertical.at(1).mach,vertical.at(1).speed/340.3,.02));
    rocket.launchTiltDeg=4;
    const auto east=simulate(rocket,c6);
    assert(east.valid && east.landed && east.maxRange>20 &&
           east.at(5).position.x>20 && near(east.at(5).position.z,0,1e-5));
    rocket.launchHeadingDeg=90;
    const auto north=simulate(rocket,c6);
    assert(north.valid && north.at(5).position.z>20 &&
           near(north.at(5).position.x,0,1e-5));
    rocket.launchTiltDeg=0;rocket.launchHeadingDeg=0;
    rocket.gimbalPitchDeg=.5;
    const auto gimballed=simulate(rocket,c6);
    assert(gimballed.valid && gimballed.at(.5).tiltDeg>5 &&
           gimballed.at(2).position.x < -5);
    rocket.gimbalPitchDeg=0;rocket.gimbalYawDeg=.5;
    const auto yawed=simulate(rocket,c6);
    assert(yawed.valid && yawed.at(2).position.z < -5);
    rocket.gimbalYawDeg=0;rocket.windEast=2;
    const auto drift=simulate(rocket,c6);
    assert(drift.valid && std::fabs(drift.at(drift.apogeeTime).position.x)>1 &&
           drift.maxRange>vertical.maxRange+1);
    const auto mixed=drift.at(.333);
    const double qnorm=mixed.orientation.w*mixed.orientation.w+
        mixed.orientation.x*mixed.orientation.x+
        mixed.orientation.y*mixed.orientation.y+
        mixed.orientation.z*mixed.orientation.z;
    assert(near(qnorm,1,1e-8) && near(mixed.range,
           std::hypot(mixed.position.x,mixed.position.z)));
    rocket=Rocket{};
    auto weak = from("WEAK 18 70 P .01 .02 TEST\n1 0.5\n2 0\n");
    assert(simulate(rocket, weak).valid && simulate(rocket,weak).apogee==0);
    auto falling = simulate(rocket, weak, 50.0);
    assert(falling.valid && falling.at(0).acceleration < 0 && falling.landed);
    std::printf("PASS core: Klima C6 %.1f m model apogee, %.1f m/s model touchdown, %.1f N*s (manufacturer curve; not flight-rated)\n",
        flight.apogee,flight.touchdownSpeed,c6.totalImpulse());
}
