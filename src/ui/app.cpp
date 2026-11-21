#include "app.hpp"
#include "workbench_layout.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace eulo {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr int counts[] = {6,5,9,2,6,4,7,8,5};
constexpr int benchProfileFields=10;
unsigned benchValue(const bench::Profile& p,int field) {
    switch(field) {
        case 0:return static_cast<unsigned>(p.family);
        case 1:return p.vendor;
        case 2:return p.product;
        case 3:return p.interfaceNumber;
        case 4:return p.bulkOut;
        case 5:return p.bulkIn;
        default:return p.pins[static_cast<std::size_t>(field-6)];
    }
}
std::string number(double value,int decimals) {
    char text[64];
    std::snprintf(text,sizeof(text),"%.*f",decimals,value);
    return text;
}
}

void App::setBenchControl(BenchControl* control) {
    if(bench_ && bench_!=control)bench_->close();
    bench_=control;
    benchConfirm_=false;
}
void App::leaveBench() {
    benchConfirm_=false;benchHexEditing_=false;benchSetup_=false;
    if(page_==Page::Bench && bench_)bench_->close();
}
bool App::setBenchProfileField(int index,unsigned value) {
    if(index<0 || index>=benchProfileFields ||
       value>(index==1 || index==2?0xffffu:0xffu)) {
        status_="Bench profile value is outside its field range.";return false;
    }
    bench::Profile candidate=benchProfile_;
    switch(index) {
        case 0:
            if(value>3) {status_="Select a known board-family label.";return false;}
            candidate.family=static_cast<bench::Family>(value);break;
        case 1:candidate.vendor=static_cast<uint16_t>(value);break;
        case 2:candidate.product=static_cast<uint16_t>(value);break;
        case 3:candidate.interfaceNumber=static_cast<uint8_t>(value);break;
        case 4:case 5:
            if(value && ((value&0x70)!=0 || (value&0x0f)==0 ||
               ((value&0x80)!=0)!=(index==5))) {
                status_="Select a bulk endpoint 01..0F OUT / 81..8F IN, or 00 Auto.";
                return false;
            }
            if(index==4)candidate.bulkOut=static_cast<uint8_t>(value);
            else candidate.bulkIn=static_cast<uint8_t>(value);
            break;
        default:
            if(value!=bench::unsetPin &&
               ((candidate.family==bench::Family::Esp32 && value>48) ||
                (candidate.family==bench::Family::Stm32 && value>0x7f))) {
                status_="Pin code exceeds the chosen board family range.";return false;
            }
            candidate.pins[static_cast<std::size_t>(index-6)]=static_cast<uint8_t>(value);
    }
    if(benchValue(candidate,index)==benchValue(benchProfile_,index))return true;
    // Changing any field neutralises/closes a live link before its descriptor
    // or pin interpretation can change. Saving never applies GPIO remotely.
    if(bench_)bench_->close();
    benchConfirm_=false;
    benchProfile_=candidate;benchProfileDirty_=true;
    if(index<=5)benchCapabilitiesSeen_=false; // cached pool belongs to old USB identity
    status_="Bench profile edited locally. Save, then identify the board.";
    return true;
}
void App::beginBenchHexEdit() {
    if(!benchSetup_ || benchSetupSelected_<0 ||
       benchSetupSelected_>=benchProfileFields)return;
    if(benchSetupSelected_==0) {
        setBenchProfileField(0,(benchValue(benchProfile_,0)+1)%4);
        return;
    }
    const int width=(benchSetupSelected_==1 || benchSetupSelected_==2)?4:2;
    char buffer[8];
    std::snprintf(buffer,sizeof(buffer),"%0*X",width,
                  benchValue(benchProfile_,benchSetupSelected_));
    benchHexBuffer_=buffer;benchHexNibble_=0;benchHexEditing_=true;
    status_="Edit hex: left/right digit, up/down value, X/Enter use, O/Esc cancel.";
}
void App::benchHexDigit(char ch) {
    if(!benchHexEditing_ || benchHexBuffer_.empty())return;
    if(ch>='a' && ch<='f')ch=static_cast<char>(ch-'a'+'A');
    if(!((ch>='0' && ch<='9') || (ch>='A' && ch<='F')))return;
    benchHexBuffer_[static_cast<std::size_t>(benchHexNibble_)]=ch;
    benchHexNibble_=std::min(benchHexNibble_+1,
                             static_cast<int>(benchHexBuffer_.size())-1);
}
void App::commitBenchHexEdit() {
    if(!benchHexEditing_)return;
    const unsigned value=static_cast<unsigned>(std::strtoul(benchHexBuffer_.c_str(),nullptr,16));
    if(setBenchProfileField(benchSetupSelected_,value)) {
        benchHexEditing_=false;
        status_="Profile value set locally. Save profile before connecting.";
    }
}
void App::cancelBenchHexEdit() {
    benchHexEditing_=false;benchHexBuffer_.clear();
    status_="Profile edit cancelled. No board setting changed.";
}
bool App::init(const char* fontPath,const char* motorDirectory,const char* projectPath) {
    if (!font_.load(fontPath) || !motorDirectory) return false;
    projectPath_ = projectPath ? projectPath : "project.eul";
    const size_t slash=projectPath_.find_last_of('/');
    const std::string directoryPrefix=slash==std::string::npos?std::string():
                                      projectPath_.substr(0,slash+1);
    recordingPath_=directoryPrefix+"recording.csv";
    benchProfilePath_=directoryPrefix+"bench-profile.cfg";
    std::string profileError;
    benchProfileSaved_=bench::loadProfile(benchProfile_,benchProfilePath_.c_str(),
                                          profileError);
    if(!benchProfileSaved_)benchProfile_={}; // fail closed on malformed files
    std::string error;
    const std::string directory(motorDirectory);
    if (!motors_[0].load((directory+"/Klima_C6.eng").c_str(),error) ||
        !motors_[1].load((directory+"/Klima_D9.eng").c_str(),error)) {
        status_ = error;
        return false;
    }
    rebuild();
    return true;
}
void App::rebuild() { flight_=simulate(rocket_,motor()); }
std::string App::projectDirectory() const {
    const size_t slash=projectPath_.find_last_of('/');
    return slash==std::string::npos?std::string():projectPath_.substr(0,slash+1);
}
bool App::loadReference(int format,ReferenceMesh& mesh,std::string& error) const {
    if(format!=1 && format!=2) {error="Select OBJ or STL.";return false;}
    const std::string path=projectDirectory()+(format==1?"reference.obj":"reference.stl");
    return mesh.load(path.c_str(),error);
}
bool App::importReference(int format) {
    leaveBench();
    ReferenceMesh candidate;
    std::string error;
    if(!loadReference(format,candidate,error)) {status_=error;return false;}
    const Rocket before=rocket_;
    rocket_.referenceFormat=format;
    reference_=std::move(candidate);
    if(before.referenceFormat!=format) {
        previousRocket_=before;hasUndo_=true;dirty_=true;
    }
    page_=Page::Garage;selectedPart_=8;selectedField_=0;focusRegion_=3;playing_=false;
    status_="Reference mesh loaded for visual comparison; no mass or solver coupling.";
    return true;
}
bool App::importTelemetry(const char* path) {
    leaveBench();
    const std::string source=path?path:"";
    TelemetryRecording next;
    std::string error;
    if(!next.load(source.c_str(),error)) {status_=error;return false;}
    recording_=std::move(next);
    recordingPath_=source;
    groundTime_=recording_.samples.front().time;
    page_=Page::Ground;playing_=false;focusRegion_=4;
    status_=recording_.hasPressure()?
        "Barometer/IMU CSV loaded; device origin unverified. Calibrate first 2 s.":
        "Legacy altitude/gyro CSV loaded; origin unverified. Calibrate first 2 s.";
    return true;
}
int App::fieldCount(int part) const { return part>=0 && part<9 ? counts[part] : 0; }

App::Field App::field(int part,int index) const {
    const Rocket& r=rocket_;
    switch (part) {
        case 0:
            switch(index) {
                case 0:return {"Length","mm",r.bodyLength*1000,200,3000,10,0,false};
                case 1:return {"Outer diameter","mm",r.radius*2000,20,200,1,1,false};
                case 2:return {"Wall thickness","mm",r.wall*1000,0.4,10,0.1,1,false};
                case 3:return {"Material","",static_cast<double>(static_cast<int>(r.material)),0,2,1,0,true};
                case 4:return {"Density","kg/m3",r.bodyDensity,80,8000,50,0,false};
                case 5:return {"Drag coefficient","Cd",r.airframeCd,0.1,2.0,0.05,2,false};
            } break;
        case 1:
            switch(index) {
                case 0:return {"Length","mm",r.noseLength*1000,20,600,5,0,false};
                case 1:return {"Profile","",static_cast<double>(static_cast<int>(r.noseProfile)),0,1,1,0,true};
                case 2:return {"Shell thickness","mm",r.noseWall*1000,0.4,10,0.1,1,false};
                case 3:return {"Material","",static_cast<double>(static_cast<int>(r.noseMaterial)),0,2,1,0,true};
                case 4:return {"Density","kg/m3",r.noseDensity,80,8000,50,0,false};
            } break;
        case 2:
            switch(index) {
                case 0:return {"Fin count","",static_cast<double>(r.finCount),2,8,1,0,false};
                case 1:return {"Span","mm",r.finSpan*1000,10,250,1,0,false};
                case 2:return {"Root chord","mm",r.finRootChord*1000,20,500,2,0,false};
                case 3:return {"Tip chord","mm",r.finTipChord*1000,10,400,2,0,false};
                case 4:return {"Sweep","mm",r.finSweep*1000,0,250,2,0,false};
                case 5:return {"Thickness","mm",r.finThickness*1000,1,12,0.1,1,false};
                case 6:return {"Setback","mm",r.finSetback*1000,0,500,2,0,false};
                case 7:return {"Material","",static_cast<double>(static_cast<int>(r.finMaterial)),0,2,1,0,true};
                case 8:return {"Density","kg/m3",r.finDensity,80,8000,20,0,false};
            } break;
        case 3:
            switch(index) {
                case 0:return {"Assembly mass","g",r.electronicsMass*1000,0,1900,1,0,false};
                case 1:return {"CG from tail","mm",r.electronicsFromTail*1000,0,3000,5,0,false};
            } break;
        case 4:
            switch(index) {
                case 0:return {"Published curve","",static_cast<double>(r.motorIndex),0,1,1,0,true};
                case 1:return {"Motor CG / tail","mm",r.motorFromTail*1000,
                    motor().lengthMm/2,std::max(motor().lengthMm/2,
                        r.mountLength*1000-motor().lengthMm/2),2,0,false};
                case 2:return {"Mount length","mm",r.mountLength*1000,70,400,5,0,false};
                case 3:return {"Bore diameter","mm",r.mountBoreRadius*2000,18,50,0.5,1,false};
                case 4:return {"Sleeve wall","mm",r.mountWall*1000,0.4,5,0.1,1,false};
                case 5:return {"Sleeve density","kg/m3",r.mountDensity,80,8000,50,0,false};
            } break;
        case 5:
            switch(index) {
                case 0:return {"Canopy diameter","mm",r.parachuteDiameter*1000,0,1500,10,0,false};
                case 1:return {"Canopy drag","Cd",r.canopyCd,0.2,3.0,0.05,2,false};
                case 2:return {"Fabric density","g/m2",r.canopyArealDensity*1000,10,1000,5,0,false};
                case 3:return {"CG from tail","mm",r.recoveryFromTail*1000,0,3000,5,0,false};
            } break;
        case 6:
            switch(index) {
                case 0:return {"Build insert","",r.insertEnabled?1.0:0.0,0,1,1,0,true};
                case 1:return {"Length","mm",r.insertLength*1000,2,500,1,1,false};
                case 2:return {"Outer diameter","mm",r.insertOuterRadius*2000,2,80,0.1,1,false};
                case 3:return {"Bore diameter","mm",r.insertBoreRadius*2000,0,78,0.1,1,false};
                case 4:return {"Center / tail","mm",r.insertFromTail*1000,0,
                    r.bodyLength*1000,1,1,false};
                case 5:return {"Material","",static_cast<double>(static_cast<int>(r.insertMaterial)),
                    0,2,1,0,true};
                case 6:return {"Density","kg/m3",r.insertDensity,80,8000,50,0,false};
            } break;
        case 7:
            switch(index) {
                case 0:return {"Rail tilt","deg",r.launchTiltDeg,0,20,0.5,1,false};
                case 1:return {"Rail heading","deg",r.launchHeadingDeg,-180,180,5,0,false};
                case 2:return {"Wind east","m/s",r.windEast,-30,30,0.5,1,false};
                case 3:return {"Wind north","m/s",r.windNorth,-30,30,0.5,1,false};
                case 4:return {"Gimbal pitch","deg",r.gimbalPitchDeg,-5,5,0.1,1,false};
                case 5:return {"Gimbal yaw","deg",r.gimbalYawDeg,-5,5,0.1,1,false};
                case 6:return {"CP from tail","mm",r.cpFromTail*1000,0,
                    r.bodyLength*1000,5,0,false};
                case 7:return {"Angular damping","Nms",r.angularDamping,0,0.1,0.0005,4,false};
            } break;
        case 8:
            switch(index) {
                case 0:return {"Reference file","",static_cast<double>(r.referenceFormat),
                    0,2,1,0,true};
                case 1:return {"Scale / vertex","mm",r.referenceUnitMm,0.01,1000,.1,2,false};
                case 2:return {"Offset east","mm",r.referenceEastMm,-3000,3000,5,1,false};
                case 3:return {"Offset / tail","mm",r.referenceTailMm,-3000,3000,5,1,false};
                case 4:return {"Offset north","mm",r.referenceNorthMm,-3000,3000,5,1,false};
            } break;
    }
    return {"Unknown","",0,0,0,0,0,false};
}
std::string App::valueText(int part,int index) const {
    const Field f=field(part,index);
    if(f.choice) {
        if(part==0)return materialName(rocket_.material);
        if(part==1 && index==1)return noseProfileName(rocket_.noseProfile);
        if(part==1)return materialName(rocket_.noseMaterial);
        if(part==2)return finMaterialName(rocket_.finMaterial);
        if(part==4)return motor().name;
        if(part==6 && index==0)return rocket_.insertEnabled?"BUILT":"NOT BUILT";
        if(part==6)return materialName(rocket_.insertMaterial);
        if(part==8)return rocket_.referenceFormat==0?"NONE":
            rocket_.referenceFormat==1?"OBJ":"STL";
    }
    if(part==5 && index==0 && rocket_.parachuteDiameter==0)return "OFF";
    return number(f.value,f.decimals);
}

bool App::applyField(int part,int index,double value) {
    if(part<0 || part>=9 || index<0 || index>=fieldCount(part) || !std::isfinite(value))return false;
    const Field f=field(part,index);
    if(value<f.min-1e-7 || value>f.max+1e-7) {
        status_=std::string(f.name)+": enter "+number(f.min,1)+" to "+number(f.max,1)+
                (f.unit[0]?std::string(" ")+f.unit:"");
        return false;
    }
    if(f.choice) value=std::round(value);
    if(part==2 && index==0)value=std::round(value);
    if(part==5 && index==0 && value>0 && value<100) {
        status_="Canopy diameter: 0 (off) or at least 100 mm.";
        return false;
    }
    if(std::fabs(f.value-value)<1e-8) {
        status_="Value unchanged.";
        return true;
    }
    if(part==8 && index==0 && value!=0)
        return importReference(static_cast<int>(value));
    const Rocket before=rocket_;
    switch(part) {
        case 0:
            switch(index) {
                case 0:rocket_.bodyLength=value/1000;break;
                case 1:rocket_.radius=value/2000;break;
                case 2:rocket_.wall=value/1000;break;
                case 3:rocket_.material=static_cast<Material>(static_cast<int>(value));
                       rocket_.bodyDensity=illustrativeDensity(rocket_.material);break;
                case 4:rocket_.bodyDensity=value;break;
                case 5:rocket_.airframeCd=value;break;
            } break;
        case 1:
            switch(index) {
                case 0:rocket_.noseLength=value/1000;break;
                case 1:rocket_.noseProfile=static_cast<NoseProfile>(static_cast<int>(value));break;
                case 2:rocket_.noseWall=value/1000;break;
                case 3:rocket_.noseMaterial=static_cast<Material>(static_cast<int>(value));
                       rocket_.noseDensity=illustrativeDensity(rocket_.noseMaterial);break;
                case 4:rocket_.noseDensity=value;break;
            } break;
        case 2:
            switch(index) {
                case 0:rocket_.finCount=static_cast<int>(value);break;
                case 1:rocket_.finSpan=value/1000;break;
                case 2:rocket_.finRootChord=value/1000;break;
                case 3:rocket_.finTipChord=value/1000;break;
                case 4:rocket_.finSweep=value/1000;break;
                case 5:rocket_.finThickness=value/1000;break;
                case 6:rocket_.finSetback=value/1000;break;
                case 7:rocket_.finMaterial=static_cast<FinMaterial>(static_cast<int>(value));
                       rocket_.finDensity=illustrativeFinDensity(rocket_.finMaterial);break;
                case 8:rocket_.finDensity=value;break;
            } break;
        case 3:
            if(index==0)rocket_.electronicsMass=value/1000;
            else rocket_.electronicsFromTail=value/1000;
            break;
        case 4:
            switch(index) {
                case 0:rocket_.motorIndex=static_cast<int>(value);break;
                case 1:rocket_.motorFromTail=value/1000;break;
                case 2:rocket_.mountLength=value/1000;break;
                case 3:rocket_.mountBoreRadius=value/2000;break;
                case 4:rocket_.mountWall=value/1000;break;
                case 5:rocket_.mountDensity=value;break;
            } break;
        case 5:
            switch(index) {
                case 0:rocket_.parachuteDiameter=value/1000;break;
                case 1:rocket_.canopyCd=value;break;
                case 2:rocket_.canopyArealDensity=value/1000;break;
                case 3:rocket_.recoveryFromTail=value/1000;break;
            } break;
        case 6:
            switch(index) {
                case 0:rocket_.insertEnabled=value!=0;break;
                case 1:rocket_.insertLength=value/1000;break;
                case 2:rocket_.insertOuterRadius=value/2000;break;
                case 3:rocket_.insertBoreRadius=value/2000;break;
                case 4:rocket_.insertFromTail=value/1000;break;
                case 5:rocket_.insertMaterial=static_cast<Material>(static_cast<int>(value));
                       rocket_.insertDensity=illustrativeDensity(rocket_.insertMaterial);break;
                case 6:rocket_.insertDensity=value;break;
            } break;
        case 7:
            switch(index) {
                case 0:rocket_.launchTiltDeg=value;break;
                case 1:rocket_.launchHeadingDeg=value;break;
                case 2:rocket_.windEast=value;break;
                case 3:rocket_.windNorth=value;break;
                case 4:rocket_.gimbalPitchDeg=value;break;
                case 5:rocket_.gimbalYawDeg=value;break;
                case 6:rocket_.cpFromTail=value/1000;break;
                case 7:rocket_.angularDamping=value;break;
            } break;
        case 8:
            switch(index) {
                case 0:rocket_.referenceFormat=0;reference_=ReferenceMesh{};break;
                case 1:rocket_.referenceUnitMm=value;break;
                case 2:rocket_.referenceEastMm=value;break;
                case 3:rocket_.referenceTailMm=value;break;
                case 4:rocket_.referenceNorthMm=value;break;
            } break;
    }
    const bool geometryValid=rocket_.valid();
    const bool motorFit=geometryValid && motorFits(rocket_,motor());
    if(!geometryValid || !motorFit) {
        rocket_=before;
        status_=geometryValid?
            "Rejected: motor casing must fit fully inside the mount.":
            part==6?"Rejected: insert must clear the sleeve and fit inside the body.":
            "Rejected: dependent geometry must fit inside the body.";
        return false;
    }
    previousRocket_=before;
    hasUndo_=true;
    dirty_=true;
    rebuild();
    status_=flight_.valid?std::string(f.name)+" changed; model recalculated.":
        std::string(f.name)+" changed; virtual run exceeded solver limits.";
    return true;
}
bool App::setSelectedValue(double value) {return applyField(selectedPart_,selectedField_,value);}
void App::editField(int direction) {
    const Field f=field(selectedPart_,selectedField_);
    double next=f.value + direction*f.step;
    if(f.choice) next = next<f.min?f.max:next>f.max?f.min:next;
    else if(selectedPart_==5 && selectedField_==0) {
        if(f.value==0 && direction>0)next=100;
        else if(next<100 && direction<0)next=f.value>100?100:0;
    }
    if(!f.choice) next=std::clamp(next,f.min,f.max);
    applyField(selectedPart_,selectedField_,next);
}
void App::resetSelectedPart() {
    const Rocket before=rocket_,defaults{};
    switch(selectedPart_) {
        case 0:
            rocket_.bodyLength=defaults.bodyLength;rocket_.radius=defaults.radius;
            rocket_.wall=defaults.wall;rocket_.material=defaults.material;
            rocket_.bodyDensity=defaults.bodyDensity;rocket_.airframeCd=defaults.airframeCd;break;
        case 1:
            rocket_.noseLength=defaults.noseLength;rocket_.noseWall=defaults.noseWall;
            rocket_.noseProfile=defaults.noseProfile;rocket_.noseMaterial=defaults.noseMaterial;
            rocket_.noseDensity=defaults.noseDensity;break;
        case 2:
            rocket_.finCount=defaults.finCount;rocket_.finSpan=defaults.finSpan;
            rocket_.finRootChord=defaults.finRootChord;rocket_.finTipChord=defaults.finTipChord;
            rocket_.finSweep=defaults.finSweep;rocket_.finThickness=defaults.finThickness;
            rocket_.finSetback=defaults.finSetback;rocket_.finMaterial=defaults.finMaterial;
            rocket_.finDensity=defaults.finDensity;break;
        case 3:
            rocket_.electronicsMass=defaults.electronicsMass;
            rocket_.electronicsFromTail=defaults.electronicsFromTail;break;
        case 4:
            rocket_.motorIndex=defaults.motorIndex;rocket_.motorFromTail=defaults.motorFromTail;
            rocket_.mountLength=defaults.mountLength;rocket_.mountBoreRadius=defaults.mountBoreRadius;
            rocket_.mountWall=defaults.mountWall;rocket_.mountDensity=defaults.mountDensity;break;
        case 5:
            rocket_.parachuteDiameter=defaults.parachuteDiameter;
            rocket_.canopyCd=defaults.canopyCd;
            rocket_.canopyArealDensity=defaults.canopyArealDensity;
            rocket_.recoveryFromTail=defaults.recoveryFromTail;break;
        case 6:
            rocket_.insertEnabled=defaults.insertEnabled;
            rocket_.insertLength=defaults.insertLength;
            rocket_.insertOuterRadius=defaults.insertOuterRadius;
            rocket_.insertBoreRadius=defaults.insertBoreRadius;
            rocket_.insertFromTail=defaults.insertFromTail;
            rocket_.insertMaterial=defaults.insertMaterial;
            rocket_.insertDensity=defaults.insertDensity;break;
        case 7:
            rocket_.launchTiltDeg=defaults.launchTiltDeg;
            rocket_.launchHeadingDeg=defaults.launchHeadingDeg;
            rocket_.windEast=defaults.windEast;
            rocket_.windNorth=defaults.windNorth;
            rocket_.gimbalPitchDeg=defaults.gimbalPitchDeg;
            rocket_.gimbalYawDeg=defaults.gimbalYawDeg;
            rocket_.cpFromTail=defaults.cpFromTail;
            rocket_.angularDamping=defaults.angularDamping;break;
        case 8:
            rocket_.referenceFormat=defaults.referenceFormat;
            rocket_.referenceUnitMm=defaults.referenceUnitMm;
            rocket_.referenceEastMm=defaults.referenceEastMm;
            rocket_.referenceTailMm=defaults.referenceTailMm;
            rocket_.referenceNorthMm=defaults.referenceNorthMm;break;
    }
    if(!rocket_.valid() || !motorFits(rocket_,motor())) {
        rocket_=before;
        status_="Reset rejected: another component depends on this geometry.";
        return;
    }
    if(selectedPart_==8)reference_=ReferenceMesh{};
    previousRocket_=before;hasUndo_=true;dirty_=true;rebuild();
    status_=std::string(workbench::partNames[selectedPart_])+" restored to defaults.";
}

void App::beginEdit() {
    const Field f=field(selectedPart_,selectedField_);
    if(f.choice || !textEntryAvailable_) {
        editField(1);
        if(!textEntryAvailable_)status_="Use the D-pad left/right to adjust; X steps once.";
        return;
    }
    editing_=true;editReplace_=true;
    editBuffer_=number(f.value,f.decimals);
    status_="Type a value in the displayed units; Enter applies, Esc cancels.";
}
void App::commitEdit() {
    if(!editing_)return;
    char* end=nullptr;
    const double input=std::strtod(editBuffer_.c_str(),&end);
    if(editBuffer_.empty() || end==editBuffer_.c_str() || *end!='\0' || !std::isfinite(input)) {
        status_="Enter a finite number, then press Enter.";
        return;
    }
    if(setSelectedValue(input)) {editing_=false;editBuffer_.clear();}
}
void App::cancelEdit() {
    editing_=false;editBuffer_.clear();status_="Edit cancelled.";
}
void App::typeCharacter(char ch) {
    if(!editing_)return;
    if((ch>='0' && ch<='9') || (ch=='.' && editBuffer_.find('.')==std::string::npos) ||
       (ch=='-' && (editReplace_ || editBuffer_.empty()))) {
        if(editReplace_)editBuffer_.clear();
        if(editBuffer_.size()<22)editBuffer_+=ch;
        editReplace_=false;
    }
}
int App::menuCount(int index) const {return index>=0 && index<5?workbench::menus[index].count:0;}
void App::runMenuItem() {
    if(!menuOpen_)return;
    const Action chosen=workbench::menus[menuIndex_].items[menuItem_].action;
    menuOpen_=false;
    action(chosen);
}

void App::action(Action a) {
    demoMode_=false;
    // An emergency neutral/close must work even over a confirmation overlay.
    if(a==Action::BenchDisconnect || a==Action::BenchNeutral) {
        benchConfirm_=false;benchHexEditing_=false;
        if(a==Action::BenchDisconnect) {
            if(bench_)bench_->close();
            status_="Bench link closed. Check board neutral before touching hardware.";
        } else if(!bench_ || !bench_->connected())status_="No identified bench board.";
        else {
            std::string error;
            status_=bench_->neutral(error)?
                "Neutral ACK received; verify the rig is stationary.":error;
        }
        return;
    }
    if(benchConfirm_) {
        switch(a) {
            case Action::Back:
                benchConfirm_=false;
                status_=benchConfirmPins_?"Pin mapping cancelled; board unchanged.":
                    "Bench pulse cancelled.";
                return;
            case Action::FocusNext:case Action::NextPart:case Action::Increase:
                benchConfirmFocus_=(benchConfirmFocus_+1)%3;return;
            case Action::FocusPrevious:case Action::PrevPart:case Action::Decrease:
                benchConfirmFocus_=(benchConfirmFocus_+2)%3;return;
            case Action::Confirm:
                if(benchConfirmFocus_==0) {
                    benchConfirm_=false;
                    status_=benchConfirmPins_?"Pin mapping cancelled; board unchanged.":
                        "Bench pulse cancelled.";
                    return;
                }
                if(benchConfirmFocus_==1) {action(Action::BenchNeutral);return;}
                benchConfirm_=false;
                if(page_!=Page::Bench || !bench_ || !bench_->connected()) {
                    status_="Bench disconnected; no board command sent.";return;
                }
                {
                    std::string error;
                    if(benchConfirmPins_)status_=bench_->applyPins(error)?
                        "Board accepted pin mapping. Close the gate and re-identify.":error;
                    else status_=bench_->pulse(static_cast<uint8_t>(benchAxis_),
                        benchAngleCdeg_,150,error)?
                        "Timed bench pulse ACK; board must return to neutral.":error;
                }
                return;
            default:return;
        }
    }
    if(benchHexEditing_) {
        switch(a) {
            case Action::Back:cancelBenchHexEdit();return;
            case Action::Confirm:commitBenchHexEdit();return;
            case Action::NextField:
                benchHexNibble_=(benchHexNibble_+1)%static_cast<int>(benchHexBuffer_.size());
                return;
            case Action::PrevField:
                benchHexNibble_=(benchHexNibble_+static_cast<int>(benchHexBuffer_.size())-1)%
                    static_cast<int>(benchHexBuffer_.size());return;
            case Action::Increase:case Action::Decrease: {
                char& nibble=benchHexBuffer_[static_cast<std::size_t>(benchHexNibble_)];
                const char* hex="0123456789ABCDEF";
                int current=0;
                while(current<16 && hex[current]!=nibble)++current;
                nibble=hex[(current+(a==Action::Increase?1:15))%16];return;
            }
            default:return;
        }
    }
    if(confirmDiscard_) {
        switch(a) {
            case Action::Back:confirmDiscard_=false;status_="Kept the current project.";return;
            case Action::FocusNext:case Action::NextPart:case Action::Increase:
                discardFocus_=(discardFocus_+1)%3;return;
            case Action::FocusPrevious:case Action::PrevPart:case Action::Decrease:
                discardFocus_=(discardFocus_+2)%3;return;
            case Action::Confirm:
                if(discardFocus_==0) {
                    confirmDiscard_=false;status_="Kept the current project.";return;
                }
                if(discardFocus_==1 && !saveProject(rocket_,projectPath_.c_str())) {
                    status_="Save failed; project unchanged. Check storage or cancel.";
                    return;
                }
                if(discardFocus_==1) {dirty_=false;saved_=true;}
                confirmDiscard_=false;
                skipDiscardOnce_=true;
                action(pendingAction_);
                return;
            default:return;
        }
    }
    if(editing_ && a!=Action::Back && a!=Action::Confirm)commitEdit();
    if(editing_) {
        if(a==Action::Back)cancelEdit();
        if(a==Action::Confirm)commitEdit();
        return;
    }
    if(helpMode_ && a!=Action::HelpControls && a!=Action::HelpModel) {
        if(a==Action::Back || a==Action::Confirm || a==Action::Menu)helpMode_=0;
        return;
    }
    if((a==Action::NewProject || a==Action::Load) && dirty_ && !skipDiscardOnce_) {
        menuOpen_=false;
        pendingAction_=a;discardFocus_=0;confirmDiscard_=true;
        status_="Unsaved changes. Save, discard or cancel.";
        return;
    }
    skipDiscardOnce_=false;
    if(menuOpen_ && a!=Action::Menu && a!=Action::Back &&
       a!=Action::NextPart && a!=Action::PrevPart &&
       a!=Action::NextField && a!=Action::PrevField &&
       a!=Action::Increase && a!=Action::Decrease && a!=Action::Confirm)menuOpen_=false;
    if(menuOpen_) {
        switch(a) {
            case Action::Menu:case Action::Back:menuOpen_=false;return;
            case Action::NextPart:case Action::Increase:
                menuIndex_=(menuIndex_+1)%5;menuItem_=0;return;
            case Action::PrevPart:case Action::Decrease:
                menuIndex_=(menuIndex_+4)%5;menuItem_=0;return;
            case Action::NextField:menuItem_=(menuItem_+1)%menuCount(menuIndex_);return;
            case Action::PrevField:
                menuItem_=(menuItem_+menuCount(menuIndex_)-1)%menuCount(menuIndex_);return;
            case Action::Confirm:runMenuItem();return;
            default:break;
        }
    }
    if(page_==Page::Bench && benchSetup_) {
        if(a==Action::Back) {action(Action::BenchSetup);return;}
        if(a==Action::NextPart || a==Action::PrevPart) {
            benchSetupSelected_=(benchSetupSelected_+
                (a==Action::NextPart?1:benchProfileFields-1))%benchProfileFields;
            focusRegion_=3;return;
        }
    }
    if(page_==Page::Bench && (a==Action::NewProject || a==Action::Garage ||
       a==Action::Flight || a==Action::Ground || a==Action::LoadRecording ||
       a==Action::ImportObj || a==Action::ImportStl || a==Action::Load ||
       a==Action::JumpApogee || a==Action::NextPart || a==Action::PrevPart ||
       a==Action::Back))leaveBench();
    switch(a) {
        case Action::NewProject:
            rocket_=Rocket{};reference_=ReferenceMesh{};rebuild();
            page_=Page::Garage;selectedPart_=0;
            selectedField_=0;focusRegion_=3;simTime_=0;playing_=false;
            dirty_=false;saved_=false;hasUndo_=false;
            status_="New project. Select any component to edit.";break;
        case Action::Garage:page_=Page::Garage;focusRegion_=3;playing_=false;
            status_="Assembly workspace.";break;
        case Action::Flight:
            if(flight_.valid && !flight_.samples.empty()) {
                page_=Page::Flight;focusRegion_=4;simTime_=0;playing_=true;
                status_="Playing the virtual 3D open-loop run (not flight-rated).";
            } else status_="Simulation unavailable: inspect geometry and motor fit.";
            break;
        case Action::Ground:
            page_=Page::Ground;focusRegion_=5;playing_=false;
            status_=recording_.samples.empty()?
                "Ground: load recorded CSV. Bench USB does not stream sensors.":
                "Ground: recorded CSV, not a live sensor connection.";
            break;
        case Action::Bench:
            if(page_==Page::Bench && bench_)bench_->close();
            page_=Page::Bench;benchSetup_=false;focusRegion_=5;benchFocus_=0;playing_=false;
            status_="USB bench: edit the profile or identify a board. No auto-connect.";
            break;
        case Action::BenchSetup:
            if(page_!=Page::Bench)break;
            if(bench_)bench_->close(); // never edit/select a profile under a live link
            benchSetup_=!benchSetup_;benchConfirm_=false;
            focusRegion_=benchSetup_?3:5;
            benchSetupSelected_=0;benchSetupFocus_=0;benchFocus_=0;
            status_=benchSetup_?"USB and pin profile is separate from the rocket model.":
                "Back to manual bench. Identify again after changing the gate.";
            break;
        case Action::BenchProfileSave: {
            if(page_!=Page::Bench || !benchSetup_)break;
            std::string error;
            if(bench::saveProfile(benchProfile_,benchProfilePath_.c_str(),error)) {
                benchProfileDirty_=false;benchProfileSaved_=true;
                status_="Bench profile saved locally; no GPIO was changed.";
            } else status_=error;
            break;
        }
        case Action::BenchConnect: {
            if(page_!=Page::Bench)break;
            if(!bench_) {status_="No Orbis bench transport in this host preview.";break;}
            std::string error;
            if(!benchProfileSaved_ || benchProfileDirty_) {
                status_="Save a valid USB/pin profile in Bench setup before identifying.";
                break;
            }
            if(!benchProfile_.validTransport(error)) {status_=error;break;}
            if(!bench_->connect(error)) {status_=error;break;}
            lastBenchCapabilities_=bench_->capabilities();
            benchCapabilitiesSeen_=true; // UI hint only; link verifies live data anew
            status_=bench_->physicalGate()?
                bench_->pinsMatched()?"Board identified and mapped; gate on. Manual pulses require confirmation.":
                    "Board identified, gate on, but pins differ; open gate and apply mapping.":
                bench_->pinsMatched()?"Board identified and mapped; gate open. Close gate and reconnect.":
                    "Board identified with gate open. Apply firmware-approved pin mapping.";
            break;
        }
        case Action::BenchApplyPins: {
            if(page_!=Page::Bench || !benchSetup_)break;
            if(!bench_ || !bench_->connected()) {
                status_="Identify the board with its physical gate open first.";break;
            }
            if(bench_->physicalGate()) {
                status_="Open the physical gate, then re-identify before applying pins.";
                break;
            }
            std::string error;
            if(!benchProfile_.valid(error)) {status_=error;break;}
            benchConfirm_=true;benchConfirmPins_=true;benchConfirmFocus_=0;
            status_="Review the board pin mapping. Default is Cancel.";
            break;
        }
        case Action::BenchAxisPrev:case Action::BenchAxisNext:
            if(page_!=Page::Bench)break;
            benchAxis_=(benchAxis_+(a==Action::BenchAxisNext?1:3))%4;
            status_="Selected bench channel; this does not move a servo.";
            break;
        case Action::BenchPulseMinus:case Action::BenchPulsePlus:
            if(page_!=Page::Bench || benchSetup_)break;
            if(!bench_ || !bench_->connected() || !bench_->physicalGate() ||
               !bench_->pinsMatched()) {
                status_="No pulse: identify with gate on and verified pin mapping.";
                break;
            }
            benchAngleCdeg_=a==Action::BenchPulsePlus?200:-200;
            benchConfirmPins_=false;benchConfirmFocus_=0;benchConfirm_=true;
            status_="Confirm a single 150 ms bench pulse, or cancel.";
            break;
        case Action::LoadRecording:
            importTelemetry(recordingPath_.c_str());break;
        case Action::CalibrateRecording: {
            std::string error;
            if(recording_.calibrate(error)) {
                status_=recording_.hasPressure()?
                    "Stationary pressure/gyro baseline set; baro height is relative.":
                    "Stationary-window gyro bias and altitude zero set (relative pose).";
            } else status_=error;
            break;
        }
        case Action::ImportObj:importReference(1);break;
        case Action::ImportStl:importReference(2);break;
        case Action::ExportObj:
            status_=exportVisualObj(rocket_,(projectDirectory()+"assembly.obj").c_str())?
                "Visual OBJ exported beside project.eul (not watertight).":
                "OBJ export failed: check project storage.";
            break;
        case Action::Back:
            if(menuOpen_)menuOpen_=false;
            else if(helpMode_)helpMode_=0;
            else if(page_!=Page::Garage) {page_=Page::Garage;focusRegion_=3;playing_=false;}
            break;
        case Action::NextPart:selectedPart_=(selectedPart_+1)%9;selectedField_=0;
            focusRegion_=2;page_=Page::Garage;break;
        case Action::PrevPart:selectedPart_=(selectedPart_+8)%9;selectedField_=0;
            focusRegion_=2;page_=Page::Garage;break;
        case Action::NextField:
            if(page_==Page::Bench && benchSetup_)
                benchSetupSelected_=(benchSetupSelected_+1)%benchProfileFields;
            else selectedField_=(selectedField_+1)%fieldCount(selectedPart_);
            focusRegion_=3;break;
        case Action::PrevField:
            if(page_==Page::Bench && benchSetup_)
                benchSetupSelected_=(benchSetupSelected_+benchProfileFields-1)%benchProfileFields;
            else selectedField_=(selectedField_+fieldCount(selectedPart_)-1)%fieldCount(selectedPart_);
            focusRegion_=3;break;
        case Action::Increase:case Action::Decrease:
            if(focusRegion_==4 && page_==Page::Ground && !recording_.samples.empty()) {
                groundTime_=std::clamp(groundTime_+(a==Action::Increase?.5:-.5),
                    recording_.samples.front().time,recording_.samples.back().time);
                playing_=false;status_="Recorded-data cursor set from timeline.";
            } else if(focusRegion_==4 && page_!=Page::Ground &&
                      page_!=Page::Bench && flight_.valid &&
                      !flight_.samples.empty()) {
                page_=Page::Flight;playing_=false;
                const double step=a==Action::Increase?.5:-.5;
                simTime_=std::clamp(simTime_+step,0.0,flight_.samples.back().time);
                status_="Flight cursor set from timeline.";
            } else if(focusRegion_==3 && page_==Page::Garage)
                editField(a==Action::Increase?1:-1);
            else if(focusRegion_==3 && page_==Page::Bench && benchSetup_) {
                if(benchSetupSelected_==0)setBenchProfileField(0,
                    (benchValue(benchProfile_,0)+(a==Action::Increase?1:3))%4);
                else status_="Press X/Enter to edit hex digits; no board command is sent.";
            }
            break;
        case Action::TogglePlay:
            if(page_==Page::Ground) {
                if(recording_.samples.empty()) {
                    status_="Load a recorded CSV before playback.";break;
                }
                if(!playing_ && groundTime_>=recording_.samples.back().time-1e-6)
                    groundTime_=recording_.samples.front().time;
                playing_=!playing_;
                status_=playing_?"Playing recorded data (not live).":"Recorded playback paused.";
                break;
            }
            if(page_!=Page::Flight) {action(Action::Flight);break;}
            if(!playing_ && flight_.valid && !flight_.samples.empty() &&
               simTime_>=flight_.samples.back().time-1e-6)simTime_=0;
            playing_=!playing_;status_=playing_?"Playing flight model.":"Playback paused.";break;
        case Action::Save:
            if(saveProject(rocket_,projectPath_.c_str())) {
                dirty_=false;saved_=true;status_="Project saved.";
            } else status_="Save failed: verify the project storage directory.";
            break;
        case Action::Load: {
            Rocket candidate;
            if(!loadProject(candidate,projectPath_.c_str())) {
                status_="Open failed: no valid saved project at this location.";
                break;
            }
            ReferenceMesh mesh;
            std::string error;
            const bool linked=candidate.referenceFormat==0 ||
                loadReference(candidate.referenceFormat,mesh,error);
            rocket_=candidate;reference_=std::move(mesh);
            rebuild();dirty_=false;saved_=true;page_=Page::Garage;
            focusRegion_=3;playing_=false;hasUndo_=false;
            status_=!linked?"Project loaded; linked reference mesh is missing or invalid.":
                flight_.valid?"Project loaded.":"Project loaded; motor clearance invalid.";
            break;
        }
        case Action::Menu:menuOpen_=true;menuItem_=0;focusRegion_=0;break;
        case Action::Confirm:
            if(focusRegion_==3) {
                if(page_==Page::Garage)beginEdit();
                else if(page_==Page::Bench && benchSetup_)beginBenchHexEdit();
                else action(Action::Garage);
            }
            else if(focusRegion_==2) {
                focusRegion_=3;
                status_=std::string("Editing ")+workbench::partNames[selectedPart_]+" properties.";
            }
            else if(focusRegion_==1)action(workbench::tools[toolbarFocus_].action);
            else if(focusRegion_==0)action(Action::Menu);
            else if(focusRegion_==4)action(Action::TogglePlay);
            else if(focusRegion_==5) {
                if(page_==Page::Bench && benchSetup_) {
                    constexpr Action buttons[]={Action::BenchConnect,Action::BenchApplyPins,
                                                Action::BenchProfileSave,Action::BenchSetup};
                    action(buttons[benchSetupFocus_]);
                } else if(page_==Page::Bench) {
                    constexpr Action buttons[]={Action::BenchConnect,Action::BenchDisconnect,
                        Action::BenchAxisPrev,Action::BenchAxisNext,
                        Action::BenchPulseMinus,Action::BenchPulsePlus,
                        Action::BenchNeutral,Action::BenchSetup};
                    action(buttons[benchFocus_]);
                } else action(page_==Page::Garage?Action::ResetPart:
                    page_==Page::Ground?(recording_.samples.empty()?Action::LoadRecording:
                                         Action::CalibrateRecording):Action::Garage);
            }
            break;
        case Action::Undo:
            if(page_==Page::Bench) {
                status_="Leave the isolated bench to edit the flight model.";break;
            }
            if(hasUndo_) {
                if(rocket_.referenceFormat!=previousRocket_.referenceFormat) {
                    ReferenceMesh mesh;
                    std::string error;
                    if(previousRocket_.referenceFormat!=0)
                        loadReference(previousRocket_.referenceFormat,mesh,error);
                    reference_=std::move(mesh);
                }
                rocket_=previousRocket_;hasUndo_=false;dirty_=true;rebuild();
                status_=rocket_.referenceFormat && reference_.triangles.empty()?
                    "Change undone; linked reference mesh cannot be opened.":
                    "Last model change undone.";
            } else status_="Nothing to undo.";
            break;
        case Action::ResetPart:
            if(page_==Page::Bench)status_="Leave the bench to reset a model component.";
            else resetSelectedPart();
            break;
        case Action::JumpApogee:
            if(flight_.valid && !flight_.samples.empty()) {
                page_=Page::Flight;focusRegion_=4;playing_=false;simTime_=flight_.apogeeTime;
                status_="At model apogee. Drag the time cursor to inspect the run.";
            } else status_="No valid flight to inspect.";
            break;
        case Action::FitView:
            if(page_==Page::Bench) {
                status_="The bench has no camera; switch to Design or Flight.";break;
            }
            zoom_=1;yawOffset_=0;pitchOffset_=0;cameraPreset_=0;
            status_="Viewport fitted to the model.";break;
        case Action::SideView:
            if(page_==Page::Bench) {
                status_="The bench has no camera; switch to Design or Flight.";break;
            }
            yawOffset_=-0.65;pitchOffset_=-0.12;cameraPreset_=1;
            status_="Side camera: drag to orbit.";break;
        case Action::TopView:
            if(page_==Page::Bench) {
                status_="The bench has no camera; switch to Design or Flight.";break;
            }
            yawOffset_=-0.65;pitchOffset_=1.28;cameraPreset_=2;
            status_="Top camera: drag to orbit.";break;
        case Action::IsoView:
            if(page_==Page::Bench) {
                status_="The bench has no camera; switch to Design or Flight.";break;
            }
            yawOffset_=0;pitchOffset_=0;cameraPreset_=0;
            status_="Isometric camera: drag to orbit.";break;
        case Action::CycleView:
            action(cameraPreset_==0?Action::SideView:
                   cameraPreset_==1?Action::TopView:Action::IsoView);break;
        case Action::HelpControls:helpMode_=1;status_="Controls reference.";break;
        case Action::HelpModel:helpMode_=2;status_="Model assumptions and limitations.";break;
        case Action::BenchDisconnect:case Action::BenchNeutral:break; // handled before overlays
        case Action::FocusNext:
            focusRegion_=page_==Page::Bench?
                benchSetup_?(focusRegion_==3?5:focusRegion_==5?0:
                            focusRegion_==0?1:3):
                (focusRegion_==5?0:focusRegion_==0?1:5):(focusRegion_+1)%6;
            break;
        case Action::FocusPrevious:
            focusRegion_=page_==Page::Bench?
                benchSetup_?(focusRegion_==3?1:focusRegion_==1?0:
                            focusRegion_==0?5:3):
                (focusRegion_==5?1:focusRegion_==1?0:5):(focusRegion_+5)%6;
            break;
    }
}
void App::tick(double seconds) {
    if(!playing_)return;
    if(page_==Page::Ground && !recording_.samples.empty()) {
        groundTime_+=std::clamp(seconds,0.0,0.25);
        if(groundTime_>=recording_.samples.back().time) {
            groundTime_=recording_.samples.back().time;playing_=false;
            status_="Recorded playback complete.";
        }
        return;
    }
    if(page_!=Page::Flight || !flight_.valid || flight_.samples.empty())return;
    simTime_+=std::clamp(seconds,0.0,0.25);
    if(simTime_>=flight_.samples.back().time) {
        simTime_=flight_.samples.back().time;playing_=false;
        status_="Playback complete; model landed or ended.";
    }
}
void App::orbit(double yawRadians,double pitchRadians) {
    if(page_==Page::Bench || benchConfirm_ || confirmDiscard_ || helpMode_)return;
    if(!std::isfinite(yawRadians) || !std::isfinite(pitchRadians))return;
    yawOffset_=std::remainder(yawOffset_+std::clamp(yawRadians,-0.2,0.2),2*pi);
    pitchOffset_=std::clamp(pitchOffset_+std::clamp(pitchRadians,-0.2,0.2),-1.47,1.32);
    if(yawRadians!=0 || pitchRadians!=0)cameraPreset_=3;
}

void App::key(const std::string& name,bool ctrl,bool shift) {
    if(page_==Page::Bench && !ctrl && (name=="n" || name=="N")) {
        action(Action::BenchNeutral);return;
    }
    if(benchHexEditing_) {
        if(name=="Escape")action(Action::Back);
        else if(name=="Enter" || name=="Space")action(Action::Confirm);
        else if(name=="ArrowLeft")action(Action::PrevField);
        else if(name=="ArrowRight")action(Action::NextField);
        else if(name=="ArrowUp")action(Action::Increase);
        else if(name=="ArrowDown")action(Action::Decrease);
        else if(name=="Backspace")benchHexDigit('0');
        else if(!ctrl && name.size()==1)benchHexDigit(name[0]);
        return;
    }
    if(benchConfirm_) {
        if(name=="Escape")action(Action::Back);
        else if(name=="Tab" || name=="ArrowRight" || name=="ArrowDown")
            action(Action::FocusNext);
        else if(name=="ArrowLeft" || name=="ArrowUp")
            action(Action::FocusPrevious);
        else if(name=="Enter" || name=="Space")action(Action::Confirm);
        return;
    }
    if(confirmDiscard_) {
        if(name=="Escape")action(Action::Back);
        else if(name=="Tab")action(shift?Action::FocusPrevious:Action::FocusNext);
        else if(name=="ArrowRight" || name=="ArrowDown")action(Action::FocusNext);
        else if(name=="ArrowLeft" || name=="ArrowUp")action(Action::FocusPrevious);
        else if(name=="Enter" || name=="Space")action(Action::Confirm);
        return;
    }
    if(ctrl) {
        if(name=="s" || name=="S") {action(Action::Save);return;}
        if(name=="o" || name=="O") {action(Action::Load);return;}
        if(name=="n" || name=="N") {action(Action::NewProject);return;}
        if(name=="z" || name=="Z") {action(Action::Undo);return;}
        if(name=="l" || name=="L") {action(Action::LoadRecording);return;}
        if(name=="m" || name=="M") {
            action(shift?Action::ImportStl:Action::ImportObj);return;
        }
        if(name=="e" || name=="E") {action(Action::ExportObj);return;}
    }
    if(editing_) {
        if(name=="Enter")commitEdit();
        else if(name=="Escape")cancelEdit();
        else if(name=="Backspace") {
            if(editReplace_)editBuffer_.clear();
            else if(!editBuffer_.empty())editBuffer_.pop_back();
            editReplace_=false;
        } else if(name.size()==1)typeCharacter(name[0]);
        return;
    }
    if(name=="Tab") {action(shift?Action::FocusPrevious:Action::FocusNext);return;}
    if(name=="F1") {action(Action::HelpControls);return;}
    if(name=="F3") {action(Action::SideView);return;}
    if(name=="F4") {action(Action::TopView);return;}
    if(name=="F5") {action(Action::IsoView);return;}
    if(name=="Alt") {action(Action::Menu);return;}
    if(name=="Space") {action(Action::Confirm);return;}
    if(name=="Escape") {action(Action::Back);return;}
    if(menuOpen_) {
        if(name=="ArrowLeft")action(Action::PrevPart);
        else if(name=="ArrowRight")action(Action::NextPart);
        else if(name=="ArrowUp")action(Action::PrevField);
        else if(name=="ArrowDown")action(Action::NextField);
        else if(name=="Enter" || name==" ")action(Action::Confirm);
        return;
    }
    if(focusRegion_==0 && (name=="ArrowLeft" || name=="ArrowRight")) {
        menuIndex_=(menuIndex_+(name=="ArrowLeft"?4:1))%5;
        return;
    }
    if(focusRegion_==0 && (name=="ArrowUp" || name=="ArrowDown")) {
        action(Action::Menu);return;
    }
    if(focusRegion_==4 && (name=="ArrowUp" || name=="ArrowDown")) {
        focusRegion_=name=="ArrowUp"?3:5;return;
    }
    if(page_==Page::Bench && benchSetup_ && focusRegion_==3) {
        if(name=="ArrowUp")action(Action::PrevField);
        else if(name=="ArrowDown")action(Action::NextField);
        else if(name=="ArrowLeft")action(Action::Decrease);
        else if(name=="ArrowRight")action(Action::Increase);
        else if(name=="Enter" || name=="Space")action(Action::Confirm);
        return;
    }
    if(focusRegion_==5 && page_==Page::Bench) {
        if(name=="ArrowLeft" || name=="ArrowUp") {
            if(benchSetup_)benchSetupFocus_=(benchSetupFocus_+3)%4;
            else benchFocus_=(benchFocus_+7)%8;
        } else if(name=="ArrowRight" || name=="ArrowDown") {
            if(benchSetup_)benchSetupFocus_=(benchSetupFocus_+1)%4;
            else benchFocus_=(benchFocus_+1)%8;
        } else if(name=="Enter" || name=="Space")action(Action::Confirm);
        return;
    }
    if(focusRegion_==5) {
        if(name=="ArrowUp")action(Action::FocusPrevious);
        else if(name=="ArrowDown")action(Action::FocusNext);
        if(name.find("Arrow")==0)return; // focused button cannot edit a model field
    }
    if(name=="ArrowUp") {
        if(focusRegion_==2)action(Action::PrevPart);
        else if(focusRegion_==1)toolbarFocus_=(toolbarFocus_+10)%11;
        else action(Action::PrevField);
    } else if(name=="ArrowDown") {
        if(focusRegion_==2)action(Action::NextPart);
        else if(focusRegion_==1)toolbarFocus_=(toolbarFocus_+1)%11;
        else action(Action::NextField);
    } else if(name=="ArrowLeft") {
        if(focusRegion_==1)toolbarFocus_=(toolbarFocus_+10)%11;
        else if(focusRegion_==2)action(Action::PrevPart);
        else action(Action::Decrease);
    } else if(name=="ArrowRight") {
        if(focusRegion_==1)toolbarFocus_=(toolbarFocus_+1)%11;
        else if(focusRegion_==2)action(Action::NextPart);
        else action(Action::Increase);
    } else if(name=="Enter" || name==" ")action(Action::Confirm);
    else if(page_!=Page::Bench && name.size()==1 &&
            name[0]>='1' && name[0]<='9') {
        selectedPart_=name[0]-'1';selectedField_=0;page_=Page::Garage;focusRegion_=2;
    }
}

void App::click(int x,int y) {
    if(x<0 || x>=1280 || y<0 || y>=720)return;
    if(benchHexEditing_) {
        if(y>=420 && y<458 && x>=354 && x<926) {
            if(x<512)action(Action::Back);
            else if(x>=702)action(Action::Confirm);
        } else if(y>=290 && y<359 && x>=480 &&
                  x<480+static_cast<int>(benchHexBuffer_.size())*70) {
            benchHexNibble_=(x-480)/70;
        } else if(x<330 || x>=950 || y<214 || y>=480)action(Action::Back);
        return;
    }
    if(benchConfirm_) {
        if(y>=420 && y<458 && x>=354 && x<924) {
            if(x<502)benchConfirmFocus_=0;
            else if(x>=512 && x<692)benchConfirmFocus_=1;
            else if(x>=702)benchConfirmFocus_=2;
            else return;
            action(Action::Confirm);
        } else if(x<330 || x>=950 || y<214 || y>=480)action(Action::Back);
        return;
    }
    if(confirmDiscard_) {
        if(y>=420 && y<458) {
            if(x>=354 && x<475)discardFocus_=0;
            else if(x>=488 && x<700)discardFocus_=1;
            else if(x>=714 && x<926)discardFocus_=2;
            else return;
            action(Action::Confirm);
        } else if(x<330 || x>=950 || y<214 || y>=480)action(Action::Back);
        return;
    }
    if(editing_) {commitEdit();if(editing_)return;}
    if(helpMode_) {
        if(x>=841 && x<911 && y>=163 && y<196)helpMode_=0;
        return;
    }
    if(menuOpen_) {
        for(int i=0;i<5;++i) {
            const auto& menu=workbench::menus[i];
            if(y>=workbench::menuY && y<workbench::menuY+workbench::menuH &&
               x>=menu.x && x<menu.x+menu.w) {
                if(menuIndex_==i)menuOpen_=false;
                else {menuIndex_=i;menuItem_=0;}
                return;
            }
        }
        const auto& menu=workbench::menus[menuIndex_];
        if(x>=menu.x && x<menu.x+238 && y>=62 && y<62+menu.count*36) {
            menuItem_=(y-62)/36;runMenuItem();return;
        }
        menuOpen_=false;
        return;
    }
    if(y>=workbench::menuY && y<workbench::menuY+workbench::menuH) {
        for(int i=0;i<5;++i) {
            const auto& menu=workbench::menus[i];
            if(x>=menu.x && x<menu.x+menu.w) {
                menuIndex_=i;menuItem_=0;menuOpen_=true;focusRegion_=0;return;
            }
        }
    }
    if(y>=workbench::toolbarY+5 && y<workbench::toolbarY+39) {
        for(int i=0;i<11;++i) {
            const auto& tool=workbench::tools[i];
            if(x>=tool.x && x<tool.x+tool.w) {
                toolbarFocus_=i;focusRegion_=1;action(tool.action);return;
            }
        }
    }
    if((page_==Page::Garage || page_==Page::Flight ||
        (page_==Page::Ground && recording_.calibrated)) &&
       y>=workbench::viewY+5 && y<workbench::viewY+33) {
        for(int i=0;i<3;++i) {
            const int start=workbench::viewX+467+i*68;
            if(x>=start && x<start+62) {
                action(i==0?Action::SideView:i==1?Action::TopView:Action::IsoView);
                return;
            }
        }
    }
    if(page_==Page::Bench) {
        if(benchSetup_) {
            if(x>=976 && x<1264 && y>=178 && y<178+benchProfileFields*38) {
                benchSetupSelected_=(y-178)/38;focusRegion_=3;
                if(x>=1075)beginBenchHexEdit();
            } else if(x>=976 && x<1264 && y>=585 && y<618) {
                benchSetupFocus_=2;focusRegion_=5;action(Action::BenchProfileSave);
            } else if(x>=976 && x<1264 && y>=635 && y<668) {
                benchSetupFocus_=3;focusRegion_=5;action(Action::BenchSetup);
            } else if(y>=486 && y<519 && x>=270 && x<934) {
                benchSetupFocus_=x<570?0:1;focusRegion_=5;
                action(x<570?Action::BenchConnect:Action::BenchApplyPins);
            }
        } else if(x>=976 && x<1264) {
            if(y>=318 && y<351) {benchFocus_=0;focusRegion_=5;action(Action::BenchConnect);}
            else if(y>=359 && y<392) {benchFocus_=1;focusRegion_=5;action(Action::BenchDisconnect);}
            else if(y>=410 && y<443) {
                benchFocus_=x<1120?2:3;focusRegion_=5;
                action(x<1120?Action::BenchAxisPrev:Action::BenchAxisNext);
            } else if(y>=470 && y<503) {
                benchFocus_=x<1120?4:5;focusRegion_=5;
                action(x<1120?Action::BenchPulseMinus:Action::BenchPulsePlus);
            } else if(y>=530 && y<563) {
                benchFocus_=6;focusRegion_=5;action(Action::BenchNeutral);
            } else if(y>=584 && y<617) {
                benchFocus_=7;focusRegion_=5;action(Action::BenchSetup);
            }
        }
        return;
    }
    if(x<workbench::treeW && y>=workbench::treeRowY &&
       y<workbench::treeRowY+9*workbench::treeRowH) {
        selectedPart_=(y-workbench::treeRowY)/workbench::treeRowH;
        selectedField_=0;page_=Page::Garage;focusRegion_=2;playing_=false;
        status_=std::string(workbench::partNames[selectedPart_])+" selected.";
        return;
    }
    if(x>=workbench::inspectorX && y>=workbench::fieldY &&
       y<workbench::fieldY+workbench::fieldH*fieldCount(selectedPart_) && page_==Page::Garage) {
        selectedField_=(y-workbench::fieldY)/workbench::fieldH;
        focusRegion_=3;
        if((y-workbench::fieldY)%workbench::fieldH>=19) {
            if(x>=workbench::inputX && x<workbench::inputX+28)editField(-1);
            else if(x>=workbench::inputX+workbench::inputW-28 &&
                    x<workbench::inputX+workbench::inputW)editField(1);
            else if(x>=workbench::inputX && x<workbench::inputX+workbench::inputW)beginEdit();
        }
        return;
    }
    if(x>=workbench::inspectorX+16 && x<1264 && y>=635 && y<669) {
        focusRegion_=5;
        action(page_==Page::Garage?Action::ResetPart:
               page_==Page::Ground?(recording_.samples.empty()?Action::LoadRecording:
                                    Action::CalibrateRecording):Action::Garage);
        return;
    }
    if(x>=314 && x<=919 && y>=604 && y<=660) {
        const double fraction=std::clamp((x-314)/605.0,0.0,1.0);
        if(page_==Page::Ground && !recording_.samples.empty()) {
            groundTime_=recording_.samples.front().time+fraction*
                (recording_.samples.back().time-recording_.samples.front().time);
            playing_=false;focusRegion_=4;
            status_="Recorded-data cursor set from timeline.";
        } else if(page_!=Page::Ground && flight_.valid && !flight_.samples.empty()) {
            simTime_=fraction*flight_.samples.back().time;
            page_=Page::Flight;
            playing_=false;focusRegion_=4;status_="Flight cursor set from timeline.";
        }
    }
}
void App::hover(int x,int y) {
    hoverX_=x>=0 && x<1280?x:-1;
    hoverY_=y>=0 && y<720?y:-1;
}
void App::wheel(int x,int y,int direction) {
    if(editing_) {commitEdit();if(editing_)return;}
    if(menuOpen_ || helpMode_ || confirmDiscard_ || benchConfirm_ ||
       page_==Page::Bench)return;
    if(x>=workbench::viewX && x<workbench::inspectorX &&
       y>=workbench::viewY && y<workbench::analysisY) {
        zoom_=std::clamp(zoom_+(direction>0?0.1:-0.1),0.65,1.65);
        status_="Viewport zoom changed.";
    } else if(x>=workbench::inspectorX && page_==Page::Garage) {
        selectedField_=(selectedField_+(direction<0?1:fieldCount(selectedPart_)-1))%
            fieldCount(selectedPart_);
        focusRegion_=3;
    }
}

void App::demoAt(double seconds) {
    leaveBench();
    const int stage=seconds<6?0:seconds<12?1:seconds<18?2:seconds<26?3:
        seconds<34?4:seconds<44?5:seconds<73?6:7;
    if(stage!=demoStage_) {
        demoStage_=stage;
        rocket_=Rocket{};
        reference_=ReferenceMesh{};
        if(stage>=1) {rocket_.material=Material::CarbonLaminate;rocket_.bodyDensity=1500;}
        if(stage>=2) {rocket_.noseProfile=NoseProfile::TangentOgive;rocket_.noseLength=0.115;}
        if(stage>=3) {
            rocket_.finCount=4;rocket_.finSpan=0.058;
            rocket_.finTipChord=0.047;rocket_.finSweep=0.018;
        }
        if(stage>=4) {rocket_.electronicsMass=0.023;rocket_.electronicsFromTail=0.190;}
        if(stage>=5) {rocket_.motorIndex=1;rocket_.mountLength=0.09;
            rocket_.motorFromTail=0.043;rocket_.parachuteDiameter=0.42;
            rocket_.canopyCd=1.35;}
        selectedPart_=stage==0?0:stage==1?0:stage==2?1:stage==3?2:
            stage==4?3:stage==5?5:4;
        selectedField_=stage==1?3:stage==2?1:stage==3?0:stage==4?0:0;
        page_=stage==6?Page::Flight:stage==7?Page::Ground:Page::Garage;
        dirty_=stage>0;saved_=false;hasUndo_=false;playing_=false;
        rebuild();
        status_=stage==0?"Assembly workspace. Choose any property to edit.":
            stage==1?"Body: material and density update the mass model.":
            stage==2?"Nose: tangent ogive affects the mesh and shell mass.":
            stage==3?"Fin set: count and planform are editable.":
            stage==4?"Avionics: move the mass to change the CG.":
            stage==5?"Motor and recovery settings update the flight model.":
            stage==6?"Time-compressed host replay of a calculated flight.":
            "Ground station / recorded CSV import, no live telemetry.";
    }
    demoMode_=true;
    if(stage==6 && flight_.valid && !flight_.samples.empty()) {
        const double progress=std::clamp((seconds-44)/29.0,0.0,1.0);
        const double end=flight_.samples.back().time;
        simTime_=progress<0.42?flight_.apogeeTime*progress/0.42:
            flight_.apogeeTime+(end-flight_.apogeeTime)*(progress-0.42)/0.58;
    }
}
} // namespace eulo
