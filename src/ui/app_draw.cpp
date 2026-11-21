// Native software-rendered workbench, shared by the host preview and Orbis.
// All displayed measurements come from Rocket, MotorCurve or FlightResult.
#include "app.hpp"
#include "workbench_layout.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace eulo {
namespace {
constexpr uint32_t ink=rgb(39,52,59), muted=rgb(77,93,103);
constexpr uint32_t accent=rgb(35,91,147), accentPale=rgb(222,234,244);
constexpr uint32_t chrome=rgb(242,244,243), surface=rgb(249,250,249);
constexpr uint32_t rule=rgb(199,211,215), white=rgb(255,255,255);
constexpr uint32_t dark=rgb(40,56,65);
std::string formatted(double v,int precision,const char* unit="") {
    char buffer[80];
    std::snprintf(buffer,sizeof(buffer),"%.*f%s%s",precision,v,
        unit[0]?" ":"",unit);
    return buffer;
}
std::string hexValue(unsigned value,int width) {
    char buffer[8];std::snprintf(buffer,sizeof(buffer),"%0*X",width,value);
    return buffer;
}
void rightText(Canvas& c,int right,int y,const std::string& text,uint32_t color) {
    c.text(right-c.textWidth(text),y,text,color);
}
void thinRow(Canvas& c,int x,int y,int width,const std::string& label,
             const std::string& value,bool selected=false) {
    if(selected)c.rect(x,y,width,36,accentPale);
    c.text(x+11,y+6,label,muted,0,width/2+20);
    rightText(c,x+width-12,y+6,value,ink);
    c.rect(x+11,y+35,width-22,1,rule);
}
void compactRow(Canvas& c,int x,int y,int width,const std::string& label,
                const std::string& value,bool selected=false) {
    if(selected)c.rect(x,y,width,29,accentPale);
    c.text(x+11,y+2,label,muted,0,width/2+17);
    rightText(c,x+width-12,y+2,value,ink);
    c.rect(x+11,y+29,width-22,1,rule);
}
void sectionButton(Canvas& c,int x,int y,int w,const char* label,
                   bool focused,bool hovered=false) {
    c.rect(x,y,w,33,focused?accent:hovered?accentPale:chrome);
    c.border(x,y,w,33,focused||hovered?accent:rule);
    const int width=c.textWidth(label);
    c.text(x+(w-width)/2,y+5,label,focused?white:ink);
}
} // namespace

void App::drawChrome(Canvas& c) const {
    using namespace workbench;
    c.rect(0,0,1280,32,dark);
    c.text(15,1,"EuloLab",white);
    c.rect(101,8,1,17,rgb(130,151,160));
    c.text(113,1,"ROCKET STUDIO",rgb(215,227,231));
    const std::string path=projectPath_.substr(projectPath_.find_last_of('/')==std::string::npos?
        0:projectPath_.find_last_of('/')+1);
    const std::string project=path+(dirty_?"  * unsaved":saved_?"  / saved":"  / new");
    rightText(c,1259,1,project,rgb(215,227,231));

    c.rect(0,menuY,1280,menuH,surface);
    c.rect(0,menuY+menuH-1,1280,1,rule);
    for(int i=0;i<5;++i) {
        const Menu& m=menus[i];
        const bool over=hoverY_>=menuY && hoverY_<menuY+menuH &&
            hoverX_>=m.x && hoverX_<m.x+m.w;
        if((menuOpen_ && i==menuIndex_) || over)
            c.rect(m.x-1,menuY,m.w,menuH-1,accentPale);
        c.text(m.x+8,menuY+3,m.label,(menuOpen_&&i==menuIndex_)||over?accent:ink);
        if(focusRegion_==0 && !menuOpen_ && i==menuIndex_)
            c.rect(m.x+6,menuY+menuH-4,m.w-15,2,accent);
    }
    rightText(c,1257,menuY+3,"SI units  /  educational analysis",muted);

    c.rect(0,toolbarY,1280,toolbarH,chrome);
    c.rect(0,toolbarY+toolbarH-1,1280,1,rule);
    for(int i=0;i<11;++i) {
        const Tool& t=tools[i];
        const bool active=(t.action==Action::Garage && page_==Page::Garage) ||
            (t.action==Action::Flight && page_==Page::Flight) ||
            (t.action==Action::Ground && page_==Page::Ground) ||
            (t.action==Action::Bench && page_==Page::Bench);
        const bool focused=focusRegion_==1 && i==toolbarFocus_;
        const bool over=hoverX_>=t.x && hoverX_<t.x+t.w &&
            hoverY_>=toolbarY+5 && hoverY_<toolbarY+39;
        const bool disabled=(t.action==Action::Undo && !hasUndo_) ||
            (t.action==Action::JumpApogee && !flight_.valid) ||
            (t.action==Action::TogglePlay &&
             (page_==Page::Ground?recording_.samples.empty():!flight_.valid));
        c.rect(t.x,toolbarY+5,t.w,34,active||over?accentPale:surface);
        c.border(t.x,toolbarY+5,t.w,34,focused||over?accent:rule,focused?2:1);
        c.text(t.x+(t.w-c.textWidth(t.label))/2,toolbarY+10,t.label,
               disabled?muted:active?accent:ink,0,t.w-10);
    }
    rightText(c,1261,toolbarY+10,"Tab: focus   /   F1: controls",muted);
}

void App::drawTree(Canvas& c) const {
    using namespace workbench;
    c.rect(0,contentY,treeW,statusY-contentY,surface);
    c.rect(treeW-1,contentY,1,statusY-contentY,rule);
    if(page_==Page::Bench) {
        c.text(16,111,benchSetup_?"USB profile":"USB bench",ink,1);
        c.text(17,145,benchSetup_?"Local hardware profile":
                                  "One timed pulse / USB",muted);
        c.rect(16,168,212,1,rule);
        c.text(17,185,"Transport",ink);
        c.text(17,222,bench_ && bench_->connected()?"Board identified":
            "No board connected",bench_ && bench_->connected()?accent:muted);
        c.text(17,259,bench_ && bench_->connected()?
            bench_->physicalGate()?"Gate at handshake: enabled":
                                   "Gate at handshake: open":"Gate: not checked",muted,0,211);
        c.rect(16,305,212,1,rule);
        c.text(17,319,benchSetup_?"Chosen profile":"Configured hardware",ink);
        c.text(17,354,benchSetup_?std::string(bench::familyName(benchProfile_.family)):
                                usbStatus_,muted,0,211);
        if(benchSetup_)c.text(17,385,hexValue(benchProfile_.vendor,4)+":"+
                                      hexValue(benchProfile_.product,4),ink);
        c.rect(16,416,212,1,rule);
        c.text(17,430,"Separated from flight",ink);
        c.text(17,469,benchSetup_?"Gate fixed on board":
                                    "No model-driven output",muted,0,211);
        c.text(17,504,benchSetup_?"Approved pins only":
                                    "No live gyro / altitude",muted,0,211);
        c.text(17,645,"Bench only / no radio",muted);
        return;
    }
    if(page_==Page::Ground) {
        c.text(16,111,"Data sources",ink,1);
        c.text(17,144,"Recorded / not live",muted);
        c.rect(15,163,treeW-31,1,rule);
        c.text(17,176,"CSV recording",ink);
        const size_t slash=recordingPath_.find_last_of('/');
        const std::string filename=recordingPath_.substr(slash==std::string::npos?
            0:slash+1);
        c.text(17,207,filename,muted,0,211);
        c.text(17,242,recording_.samples.empty()?"No file loaded":
            std::to_string(recording_.samples.size())+" recorded samples",ink);
        if(!recording_.samples.empty())
            c.text(17,276,formatted(recording_.samples.back().time-
                recording_.samples.front().time,1,"s duration"),muted);
        c.rect(16,318,212,1,rule);
        c.text(17,332,"Calibration",ink);
        c.text(17,368,recording_.calibrated?"Stationary window set":
            "Not calibrated",recording_.calibrated?accent:muted);
        c.text(17,400,"Pose is relative",muted);
        c.rect(16,439,212,1,rule);
        c.text(17,452,"USB bench / separate",ink);
        if(c.textWidth(usbStatus_)<=210)c.text(17,486,usbStatus_,muted);
        else {
            const size_t space=usbStatus_.find_last_of(' ',19);
            const size_t split=space==std::string::npos?19:space;
            c.text(17,480,usbStatus_.substr(0,split),muted,0,210);
            c.text(17,503,usbStatus_.substr(split+1),muted,0,210);
        }
        c.text(17,538,"No live sensor stream",ink);
        c.text(17,570,"Manual bench in View > Servo bench",muted,0,211);
        c.text(17,644,"CSV not saved in .eul",muted,0,212);
        return;
    }
    c.text(16,111,"Assembly",ink,1);
    c.text(17,144,"7 parts + setup + mesh",muted,0,210);
    c.rect(15,163,treeW-31,1,rule);
    for(int i=0;i<9;++i) {
        const int y=treeRowY+i*treeRowH;
        if(i==selectedPart_) {
            c.rect(0,y,treeW-1,treeRowH,accentPale);
            c.rect(0,y,4,treeRowH,accent);
        } else if(hoverX_>=0 && hoverX_<treeW && hoverY_>=y && hoverY_<y+treeRowH) {
            c.rect(0,y,treeW-1,treeRowH,rgb(234,240,242));
        }
        c.text(17,y+7,"0"+std::to_string(i+1),i==selectedPart_?accent:muted);
        c.text(53,y+7,partNames[i],i==selectedPart_?ink:dark,0,153);
        rightText(c,treeW-17,y+7,std::to_string(fieldCount(i)),muted);
        c.rect(17,y+treeRowH-1,treeW-33,1,rule);
    }
    if(focusRegion_==2 && !menuOpen_ && !helpMode_)
        c.border(7,treeRowY+selectedPart_*treeRowH+3,treeW-16,treeRowH-6,accent,2);
    const MassProperties mass=massProperties(rocket_,motor().loadedKg,motor().propellantKg);
    c.text(16,500,"Mass properties",ink);
    c.rect(16,526,treeW-32,1,rule);
    thinRow(c,7,532,treeW-14,"Loaded",formatted(mass.loaded*1000,1,"g"));
    thinRow(c,7,568,treeW-14,"Dry",formatted(mass.dry*1000,1,"g"));
    thinRow(c,7,604,treeW-14,"CG / tail",formatted(mass.cgFromTail*1000,0,"mm"));
    c.text(16,646,"Illustrative mass model",muted,0,treeW-30);
}

void App::drawInspector(Canvas& c) const {
    using namespace workbench;
    c.rect(inspectorX,contentY,320,statusY-contentY,surface);
    c.rect(inspectorX,contentY,1,statusY-contentY,rule);
    c.text(inspectorX+16,111,page_==Page::Garage?"Properties":
        page_==Page::Flight?"Flight analysis":
        page_==Page::Bench?(benchSetup_?"USB and pin setup":"Manual actuators"):
                            "Recorded sensors",ink,1);
    if(page_==Page::Bench) {
        if(benchSetup_) {
            c.text(inspectorX+16,148,benchProfileDirty_?"Local edits / not saved":
                benchProfileSaved_?"Saved profile / no auto-connect":
                                    "No profile / USB disabled",muted,0,285);
            c.rect(inspectorX+16,171,288,1,rule);
            const char* names[]={"Board family","USB VID","USB PID","Interface",
                "Bulk OUT","Bulk IN","Pitch pin","Yaw pin","Fin A pin","Fin B pin"};
            std::string values[10];
            values[0]=bench::familyName(benchProfile_.family);
            values[1]=hexValue(benchProfile_.vendor,4);
            values[2]=hexValue(benchProfile_.product,4);
            values[3]=hexValue(benchProfile_.interfaceNumber,2);
            values[4]=benchProfile_.bulkOut?hexValue(benchProfile_.bulkOut,2):"00 / auto";
            values[5]=benchProfile_.bulkIn?hexValue(benchProfile_.bulkIn,2):"00 / auto";
            for(int i=0;i<4;++i)values[6+i]=
                bench::pinName(benchProfile_.family,benchProfile_.pins[i]);
            for(int i=0;i<10;++i)compactRow(c,inspectorX+6,178+i*38,305,
                names[i],values[i],focusRegion_==3 && i==benchSetupSelected_);
            sectionButton(c,976,585,288,"Save profile locally",
                focusRegion_==5 && benchSetupFocus_==2,
                hoverX_>=976 && hoverX_<1264 && hoverY_>=585 && hoverY_<618);
            sectionButton(c,976,635,288,"Back to manual controls",
                focusRegion_==5 && benchSetupFocus_==3,
                hoverX_>=976 && hoverX_<1264 && hoverY_>=635 && hoverY_<668);
            return;
        }
        constexpr const char* axisNames[]={"TVC pitch","TVC yaw","Fin A","Fin B"};
        c.text(inspectorX+16,148,"USB / board-local watchdog",muted);
        c.rect(inspectorX+16,171,288,1,rule);
        compactRow(c,inspectorX+6,181,305,"Channel",axisNames[benchAxis_]);
        compactRow(c,inspectorX+6,219,305,"Pulse","+/- 2 deg  /  150 ms");
        compactRow(c,inspectorX+6,257,305,"Interlock",bench_ && bench_->connected()?
            bench_->physicalGate()?"At handshake: on":"At handshake: off":
                                   "Not checked");
        c.text(976,294,bench_ && bench_->connected()?
            bench_->pinsMatched()?"Axis pins match saved profile":"Axis pins not yet verified":
                                  "Axis pins: not checked",muted,0,286);
        const auto button=[&](int index,int x,int y,int w,const char* label) {
            sectionButton(c,x,y,w,label,focusRegion_==5 && benchFocus_==index,
                hoverX_>=x && hoverX_<x+w && hoverY_>=y && hoverY_<y+33);
        };
        button(0,976,318,288,"Connect / identify board");
        button(1,976,359,288,"Disconnect / request neutral");
        button(2,976,410,140,"Previous axis");
        button(3,1124,410,140,"Next axis");
        button(4,976,470,140,"Pulse -2 deg");
        button(5,1124,470,140,"Pulse +2 deg");
        button(6,976,530,288,"Request neutral");
        button(7,976,584,288,"Configure USB and pin map");
        c.text(976,637,"Independent power cut required.",muted);
        return;
    }
    if(page_==Page::Garage) {
        c.text(inspectorX+16,148,std::string("0")+std::to_string(selectedPart_+1)+
               "  /  "+partNames[selectedPart_],muted);
        c.rect(inspectorX+16,171,288,1,rule);
        for(int i=0;i<fieldCount(selectedPart_);++i) {
            const int y=fieldY+i*fieldH;
            const Field f=field(selectedPart_,i);
            const bool active=i==selectedField_;
            const bool over=hoverX_>=inputX && hoverX_<inputX+inputW &&
                hoverY_>=y+19 && hoverY_<y+39;
            if(active) {
                c.rect(inspectorX+1,y,319,fieldH-1,accentPale);
                c.rect(inspectorX+1,y,3,fieldH-1,accent);
            } else if(over)c.rect(inspectorX+1,y,319,fieldH-1,rgb(234,240,242));
            c.text(inputX,y-2,f.name,ink,0,255);
            if(f.unit[0])rightText(c,inputX+inputW-3,y-2,f.unit,muted);
            c.rect(inputX,y+19,inputW,20,white);
            c.border(inputX,y+19,inputW,20,(active&&focusRegion_==3)||over?accent:rule);
            c.rect(inputX+28,y+19,1,20,rule);
            c.rect(inputX+inputW-29,y+19,1,20,rule);
            c.text(inputX+11,y+16,f.choice?"<":"-",accent);
            c.text(inputX+inputW-18,y+16,f.choice?">":"+",accent);
            std::string value=editing_&&active?editBuffer_+"|":valueText(selectedPart_,i);
            const int valueWidth=c.textWidth(value);
            c.text(inputX+inputW/2-valueWidth/2,y+16,value,
                   editing_&&active?accent:ink,0,inputW-63);
            c.rect(inspectorX+16,y+fieldH-1,288,1,rule);
        }
        // The lower third of the inspector explains what these editable values
        // feed, rather than displaying a fictitious engineering pass/fail badge.
        const char* descriptions[][2] = {
            {"Shell / density","Geometry updates mass and CG."},
            {"Nose shell / profile","Profile affects shape and mass."},
            {"Planform / placement","Fins must fit inside the tube."},
            {"Avionics / point mass","Location shifts center of mass."},
            {"Motor / internal sleeve","Thrust data is read-only."},
            {"Idealised deployment","0 mm switches canopy off."},
            {"Coaxial cylinder or tube","Built insert adds mass and inertia."},
            {"Open-loop scenario","Illustrative CP and fin forces."},
            {"Visual comparison only","OBJ/STL is not mass or CAD solid."}
        };
        const int noteY=std::max(566,fieldY+fieldH*fieldCount(selectedPart_)+8);
        c.text(inspectorX+16,noteY,descriptions[selectedPart_][0],ink);
        c.text(inspectorX+16,noteY+24,descriptions[selectedPart_][1],muted,0,286);
        if(selectedPart_==4) {
            c.text(inspectorX+16,noteY+47,
                formatted(motor().totalImpulse(),1,"N*s")+"  /  "+
                formatted(motor().burnTime(),2,"s"),muted);
        } else if(selectedPart_==0) {
            const double gap=rocket_.radius-rocket_.wall-
                rocket_.mountBoreRadius-rocket_.mountWall;
            c.text(inspectorX+16,noteY+47,formatted(gap*1000,1,"mm sleeve clearance"),muted);
        } else if(selectedPart_==6 && rocket_.insertEnabled) {
            const double volume=3.14159265358979323846*rocket_.insertLength*
                (rocket_.insertOuterRadius*rocket_.insertOuterRadius-
                 rocket_.insertBoreRadius*rocket_.insertBoreRadius);
            c.text(inspectorX+16,noteY+47,formatted(volume*rocket_.insertDensity*1000,1,"g insert"),muted);
        } else if(selectedPart_==7) {
            c.text(inspectorX+16,noteY+47,"0 deg heading = east; +90 = north",muted,0,285);
        } else if(selectedPart_==8 && !reference_.triangles.empty()) {
            c.text(inspectorX+16,noteY+47,
                std::to_string(reference_.triangles.size())+" visual triangles",muted);
        }
        sectionButton(c,inspectorX+16,635,288,"Reset component",focusRegion_==5,
                      hoverX_>=inspectorX+16 && hoverX_<1264 && hoverY_>=635 && hoverY_<668);
    } else if(page_==Page::Flight) {
        c.text(inspectorX+16,148,"Calculated / not flight-rated",muted);
        c.rect(inspectorX+16,171,288,1,rule);
        const FlightSample state=flight_.at(simTime_);
        const struct {const char* name;std::string value;} values[] = {
            {"Time",formatted(state.time,2,"s")},
            {"Altitude",formatted(state.altitude,1,"m")},
            {"3D speed",formatted(state.speed,1,"m/s")},
            {"Mach / air",formatted(state.mach,3)},
            {"East",formatted(state.position.x,1,"m")},
            {"North",formatted(state.position.z,1,"m")},
            {"Ground range",formatted(state.range,1,"m")},
            {"Body tilt",formatted(state.tiltDeg,1,"deg")},
            {"Heading E-N",formatted(state.headingDeg,1,"deg")},
            {"Thrust",formatted(state.thrust,1,"N")},
            {"Apogee",formatted(flight_.apogee,1,"m")},
            {"Touchdown",flight_.landed?formatted(flight_.touchdownSpeed,1,"m/s"):"Not reached"}
        };
        for(int i=0;i<12;++i)compactRow(c,inspectorX+6,fieldY+i*31,305,
                                        values[i].name,values[i].value,
                                        focusRegion_==3 && i==0);
        c.text(inspectorX+16,568,"Open-loop rigid body / not rated",ink);
        c.text(inspectorX+16,591,
            formatted(state.dynamicPressure,0,"Pa q")+"  /  "+
            formatted(state.properG,1,"g proper"),muted,0,286);
        sectionButton(c,inspectorX+16,635,288,"Edit components",focusRegion_==5,
                      hoverX_>=inspectorX+16 && hoverX_<1264 && hoverY_>=635 && hoverY_<668);
    } else {
        c.text(inspectorX+16,148,"CSV file / not a USB connection",muted);
        c.rect(inspectorX+16,171,288,1,rule);
        if(recording_.samples.empty()) {
            c.text(inspectorX+16,202,"No sensor samples loaded",ink);
            c.text(inspectorX+16,242,"Header and units required:",muted);
            c.text(inspectorX+16,273,"time_s,altitude_m,...",ink,0,286);
            c.text(inspectorX+16,304,"gx_deg_s,gy_deg_s,gz_deg_s",muted,0,286);
            c.rect(inspectorX+16,345,288,1,rule);
            c.text(inspectorX+16,568,"Recorded CSV, not live data.",ink);
            c.text(inspectorX+16,591,"Place file next to project.eul.",muted);
        } else {
            const TelemetrySample s=recording_.at(groundTime_);
            const bool calibrated=recording_.calibrated;
            const Quaternion q=s.attitude;
            const double cosine=std::clamp(1-2*(q.x*q.x+q.z*q.z),-1.0,1.0);
            const double tilt=std::acos(cosine)*180.0/3.14159265358979323846;
            const struct {const char* name;std::string value;} values[] = {
                {"Time",formatted(s.time,2,"s")},
                {"Raw altitude",formatted(s.altitude,1,"m")},
                {"Zeroed alt",calibrated?formatted(s.altitude-recording_.altitudeZero,1,"m"):"--"},
                {"Gyro X",formatted(s.gyro.x,2,"deg/s")},
                {"Gyro Y",formatted(s.gyro.y,2,"deg/s")},
                {"Gyro Z",formatted(s.gyro.z,2,"deg/s")},
                {"Bias X",calibrated?formatted(recording_.gyroBias.x,2,"deg/s"):"--"},
                {"Bias Y",calibrated?formatted(recording_.gyroBias.y,2,"deg/s"):"--"},
                {"Bias Z",calibrated?formatted(recording_.gyroBias.z,2,"deg/s"):"--"},
                {"Relative tilt",calibrated?formatted(tilt,1,"deg"):"--"}
            };
            for(int i=0;i<10;++i)compactRow(c,inspectorX+6,fieldY+i*36,305,
                                            values[i].name,values[i].value);
            c.text(inspectorX+16,568,calibrated?"Bias + altitude zero set":"Calibration required",ink);
            c.text(inspectorX+16,591,"Gyro-only attitude drifts over time.",muted,0,288);
        }
        const char* button=recording_.samples.empty()?"Load recorded CSV":
            recording_.calibrated?"Recalibrate first 2 s":"Calibrate first 2 s";
        sectionButton(c,inspectorX+16,635,288,button,focusRegion_==5,
                      hoverX_>=inspectorX+16 && hoverX_<1264 && hoverY_>=635 && hoverY_<668);
    }
}

void App::drawAnalysis(Canvas& c) const {
    using namespace workbench;
    c.rect(viewX,analysisY,viewW,analysisH,surface);
    c.rect(viewX,analysisY,viewW,1,rule);
    if(page_==Page::Bench) {
        c.text(viewX+16,analysisY+7,benchSetup_?"Hardware setup":
                                              "Bench protocol",ink);
        if(benchSetup_) {
            c.text(viewX+16,analysisY+34,
                "App selects only firmware-provisioned PWM pins; GPIO gate stays fixed.",muted,0,682);
            c.text(viewX+16,analysisY+63,
                "No automatic discovery of unknown firmware or arbitrary chip GPIOs.",muted,0,682);
            c.text(viewX+16,analysisY+92,
                "Host preview has no USB bridge; console USB unverified.",muted);
        } else {
            c.text(viewX+16,analysisY+34,"One bounded command / matching ACK / no retries.",muted);
            c.text(viewX+16,analysisY+63,
                   "No live sensor stream. ACK is not position feedback.",muted);
            c.text(viewX+16,analysisY+92,"Square / N: request neutral",muted);
        }
        return;
    }
    if(page_==Page::Ground) {
        c.text(viewX+16,analysisY+5,"Station log",ink);
        c.text(viewX+120,analysisY+5,recording_.calibrated?
            "/ zeroed altitude vs time":"/ raw altitude vs time",muted);
        if(recording_.samples.empty()) {
            c.text(viewX+16,analysisY+44,"No recorded data. File > Load recorded CSV.",muted);
            c.text(viewX+16,analysisY+73,"Bench USB does not supply sensor samples.",muted);
            return;
        }
        const int x0=314,x1=919,y0=653,y1=600;
        const double start=recording_.samples.front().time;
        const double duration=recording_.samples.back().time-start;
        double lowest=recording_.samples.front().altitude;
        double highest=lowest;
        for(const auto& sample:recording_.samples) {
            lowest=std::min(lowest,sample.altitude);
            highest=std::max(highest,sample.altitude);
        }
        if(highest-lowest<1) {highest+=0.5;lowest-=0.5;}
        const auto yPos=[&](double altitude) {
            return y0-static_cast<int>((altitude-lowest)/(highest-lowest)*(y0-y1));
        };
        const double zero=recording_.calibrated?recording_.altitudeZero:0;
        c.text(viewX+16,analysisY+38,formatted(highest-zero,1,"m"),accent);
        c.text(viewX+16,analysisY+79,formatted(lowest-zero,1,"m"),muted);
        c.line(x0,y1,x0,y0,rule);c.line(x0,y0,x1,y0,rule);
        const int skip=std::max(1,static_cast<int>(recording_.samples.size()/550));
        int lastX=x0,lastY=yPos(recording_.samples.front().altitude);
        for(size_t i=static_cast<size_t>(skip);i<recording_.samples.size();
            i+=static_cast<size_t>(skip)) {
            const auto& s=recording_.samples[i];
            const int x=x0+static_cast<int>((s.time-start)/duration*(x1-x0));
            const int y=yPos(s.altitude);
            c.line(lastX,lastY,x,y,accent,2);lastX=x;lastY=y;
        }
        c.line(lastX,lastY,x1,yPos(recording_.samples.back().altitude),accent,2);
        const int cursor=x0+static_cast<int>((groundTime_-start)/duration*(x1-x0));
        c.line(cursor,y1-2,cursor,y0+4,dark);
        c.circle(cursor,yPos(recording_.at(groundTime_).altitude),4,dark);
        c.text(x0,660,formatted(start,1,"s"),muted);
        rightText(c,x1,660,formatted(recording_.samples.back().time,1,"s"),muted);
        if(focusRegion_==4)c.border(x0-4,y1-5,x1-x0+8,y0-y1+12,accent);
        return;
    }
    c.text(viewX+16,analysisY+5,"Trajectory",ink);
    c.text(viewX+107,analysisY+5,"/ altitude (blue)  ground range (grey)",muted);
    if(!flight_.valid || flight_.samples.empty()) {
        c.text(viewX+16,analysisY+53,"No model run. Adjust geometry or motor clearance.",ink);
        return;
    }
    const int x0=314,x1=919,y0=653,y1=600;
    const double end=flight_.samples.back().time;
    const double top=std::max({0.01,flight_.apogee,flight_.maxRange});
    c.text(viewX+16,analysisY+38,formatted(top,0,"m"),accent);
    c.text(viewX+16,analysisY+79,"0 m",muted);
    c.line(x0,y1,x0,y0,rule);c.line(x0,y0,x1,y0,rule);
    const int apx=x0+static_cast<int>(std::clamp(flight_.apogeeTime/end,0.0,1.0)*(x1-x0));
    c.line(apx,y1,apx,y0,rgb(197,211,219));
    const int skip=std::max(1,static_cast<int>(flight_.samples.size()/550));
    int px=x0,py=y0,rangeY=y0;
    for(size_t i=static_cast<size_t>(skip);i<flight_.samples.size();i+=static_cast<size_t>(skip)) {
        const auto& s=flight_.samples[i];
        const int x=x0+static_cast<int>((s.time/end)*(x1-x0));
        const int y=y0-static_cast<int>(s.altitude/top*(y0-y1));
        const int ry=y0-static_cast<int>(s.range/top*(y0-y1));
        c.line(px,rangeY,x,ry,rgb(112,137,148));
        c.line(px,py,x,y,accent,2);
        px=x;py=y;rangeY=ry;
    }
    const auto& last=flight_.samples.back();
    const int finalY=y0-static_cast<int>(last.altitude/top*(y0-y1));
    const int finalRangeY=y0-static_cast<int>(last.range/top*(y0-y1));
    c.line(px,rangeY,x1,finalRangeY,rgb(112,137,148));
    c.line(px,py,x1,finalY,accent,2);
    const int cursor=x0+static_cast<int>(std::clamp(simTime_/end,0.0,1.0)*(x1-x0));
    const FlightSample state=flight_.at(simTime_);
    const int cursorY=y0-static_cast<int>(state.altitude/top*(y0-y1));
    const int cursorRangeY=y0-static_cast<int>(state.range/top*(y0-y1));
    c.line(cursor,y1-2,cursor,y0+4,dark);
    c.circle(cursor,cursorRangeY,3,rgb(112,137,148));
    c.circle(cursor,cursorY,4,dark);
    c.text(x0,660,"0 s",muted);
    const std::string duration=formatted(end,1,"s");
    rightText(c,x1,660,duration,muted);
    if(focusRegion_==4) {
        c.border(x0-4,y1-5,x1-x0+8,y0-y1+12,accent);
    }
}

void App::drawStatus(Canvas& c) const {
    using namespace workbench;
    c.rect(0,statusY,1280,720-statusY,rgb(233,238,238));
    c.rect(0,statusY,1280,1,rule);
    c.rect(15,statusY+13,7,7,accent);
    c.text(31,statusY+6,status_,ink,0,615);
    c.text(697,statusY+6,page_==Page::Ground?"RECORDED DATA / NOT LIVE":
           page_==Page::Bench?"USB BENCH / NOT FLIGHT":
           "MODEL ONLY / NOT FLIGHT-RATED",muted);
    const std::string label=demoMode_?"HOST REPLAY / NOT PS4":runtimeLabel_;
    rightText(c,1262,statusY+6,label,muted);
}

void App::drawMenu(Canvas& c) const {
    if(!menuOpen_)return;
    const auto& menu=workbench::menus[menuIndex_];
    c.rect(menu.x+2,64,238,menu.count*36+3,rgb(214,222,225));
    c.rect(menu.x,62,238,menu.count*36,surface);
    c.border(menu.x,62,238,menu.count*36,rule);
    for(int i=0;i<menu.count;++i) {
        const int y=62+i*36;
        const bool over=hoverX_>=menu.x && hoverX_<menu.x+238 &&
            hoverY_>=y && hoverY_<y+36;
        if(i==menuItem_ || over)c.rect(menu.x+2,y+2,234,32,accentPale);
        c.text(menu.x+13,y+6,menu.items[i].label,i==menuItem_||over?accent:ink);
        if(i+1<menu.count)c.rect(menu.x+12,y+35,213,1,rule);
    }
}

void App::drawHelp(Canvas& c) const {
    if(!helpMode_)return;
    c.rect(0,0,1280,720,rgb(226,232,232));
    c.rect(299,151,631,420,rgb(195,207,211));
    c.rect(293,145,630,420,surface);
    c.border(293,145,630,420,rule,2);
    c.rect(293,145,630,5,accent);
    c.text(322,169,helpMode_==1?"Workbench controls":"Model limitations",ink,1);
    sectionButton(c,841,163,70,"Close",true);
    c.rect(320,218,577,1,rule);
    const char* controls[] = {
        "MOUSE + KEYBOARD  /  HOST PREVIEW",
        "Click a part, then a value to type. Enter applies; Esc cancels.",
        "Use +/- to step; drag to orbit, wheel to zoom; SIDE/TOP/ISO.",
        "Tab cycles focus; arrows navigate; Alt opens the menu.",
        "Ctrl+S saves; Ctrl+O opens; Ctrl+N resets; Ctrl+Z undoes.",
        "CONTROLLER  /  ORBIS BUILD",
        "L1/R1: part   |   L2/R2: focus   |   D-pad: select/step",
        "X: confirm/step | O: back | Square: save or neutral",
        "Options: menu/bench   |   Triangle: ground   |   R3: camera"
    };
    const char* limits[] = {
        "ILLUSTRATIVE 3D / OPEN-LOOP RIGID BODY",
        "Published Klima C6/D9 curves are read-only motor data.",
        "Wind, rail tilt and gimbal are scenario assumptions.",
        "Fin slope, CP and damping are not measured coefficients.",
        "Flight model cannot control the USB actuator bench.",
        "Canopy drag starts ideally at descent without delay.",
        "Bench USB sends manual pulses only; no live sensor data.",
        "Do not use this model to certify or plan a real flight."
    };
    const int count=helpMode_==1?9:8;
    for(int i=0;i<count;++i) {
        const char* line=helpMode_==1?controls[i]:limits[i];
        if(helpMode_==1 && page_==Page::Bench && i==8)
            line="Bench setup: X edits hex; D-pad picks digit/value; O cancels.";
        const bool header=(i==0 || (helpMode_==1 && i==5));
        c.text(321,236+i*32,line,header?accent:ink,0,578);
    }
    c.text(322,527,"Esc or Enter closes this panel.",muted);
}

void App::drawDiscardConfirmation(Canvas& c) const {
    if(!confirmDiscard_)return;
    c.rect(0,0,1280,720,rgb(227,232,232));
    c.rect(336,221,620,266,rgb(195,207,211));
    c.rect(330,214,620,266,surface);
    c.border(330,214,620,266,rule,2);
    c.rect(330,214,620,5,accent);
    c.text(356,240,"Unsaved project",ink,1);
    c.text(356,304,"You have changes that are not saved.",ink);
    c.text(356,337,pendingAction_==Action::NewProject?
        "A new project will replace the current model.":
        "Opening a project will replace the current model.",muted,0,575);
    c.rect(356,396,568,1,rule);
    const char* labels[]={"Cancel","Save and continue","Discard changes"};
    const int x[]={354,488,714},w[]={121,212,212};
    for(int i=0;i<3;++i) {
        c.rect(x[i],420,w[i],38,discardFocus_==i?accent:chrome);
        c.border(x[i],420,w[i],38,discardFocus_==i?accent:rule);
        c.text(x[i]+(w[i]-c.textWidth(labels[i]))/2,428,labels[i],
               discardFocus_==i?white:ink);
    }
}

void App::drawBenchConfirmation(Canvas& c) const {
    if(!benchConfirm_)return;
    c.rect(0,0,1280,720,rgb(226,232,232));
    c.rect(338,221,620,266,rgb(195,207,211));
    c.rect(330,214,620,266,surface);
    c.border(330,214,620,266,rule,2);
    c.rect(330,214,620,5,accent);
    c.text(355,240,benchConfirmPins_?"Confirm board pin mapping":
                                    "Confirm one physical pulse",ink,1);
    if(benchConfirmPins_) {
        c.text(355,303,"Pitch "+bench::pinName(benchProfile_.family,benchProfile_.pins[0])+
            "  /  Yaw "+bench::pinName(benchProfile_.family,benchProfile_.pins[1]),ink,0,565);
        c.text(355,337,"Fin A "+bench::pinName(benchProfile_.family,benchProfile_.pins[2])+
            "  /  Fin B "+bench::pinName(benchProfile_.family,benchProfile_.pins[3]),ink,0,565);
        c.text(355,369,"Gate OPEN. Board approves only provisioned pins. No pulse.",muted,0,565);
    } else {
        constexpr const char* names[]={"TVC pitch","TVC yaw","Fin A","Fin B"};
        const std::string command=std::string(names[benchAxis_])+": "+
            (benchAngleCdeg_>0?"+2":"-2")+" deg for 150 ms";
        c.text(355,303,command,ink);
        c.text(355,337,"No motor or propellant. Restrain the rig; use a power cut-off.",muted,0,565);
        c.text(355,369,"The board must return to neutral on timeout or gate loss.",muted,0,565);
    }
    c.rect(355,402,565,1,rule);
    sectionButton(c,354,420,148,"Cancel",benchConfirmFocus_==0);
    sectionButton(c,512,420,180,"Neutral now",benchConfirmFocus_==1);
    sectionButton(c,702,420,222,benchConfirmPins_?"Apply pin map":"Send one pulse",
                  benchConfirmFocus_==2);
}

void App::drawBenchHexEditor(Canvas& c) const {
    if(!benchHexEditing_)return;
    c.rect(0,0,1280,720,rgb(226,232,232));
    c.rect(338,221,620,266,rgb(195,207,211));
    c.rect(330,214,620,266,surface);
    c.border(330,214,620,266,rule,2);
    c.rect(330,214,620,5,accent);
    c.text(355,240,"Edit local bench profile / hex",ink,1);
    const char* names[]={"Board family","USB VID","USB PID","Interface",
        "Bulk OUT","Bulk IN","Pitch pin","Yaw pin","Fin A pin","Fin B pin"};
    c.text(355,281,names[benchSetupSelected_],muted);
    for(std::size_t i=0;i<benchHexBuffer_.size();++i) {
        const int x=480+static_cast<int>(i)*70;
        const bool selected=static_cast<int>(i)==benchHexNibble_;
        c.rect(x,303,57,57,selected?accentPale:white);
        c.border(x,303,57,57,selected?accent:rule,selected?2:1);
        const std::string digit(1,benchHexBuffer_[i]);
        c.text(x+19,316,digit,selected?accent:ink,1);
    }
    c.text(355,372,"D-pad: choose digit / change hex. Enter or X uses value.",muted,0,565);
    c.rect(355,402,565,1,rule);
    sectionButton(c,354,420,158,"Cancel",false);
    sectionButton(c,702,420,222,"Use hex value",false);
}

void App::draw(Canvas& c,double /*now*/) const {
    using namespace workbench;
    c.clear(chrome);
    if(page_==Page::Bench) {
        c.rect(viewX,viewY,viewW,viewH,rgb(235,240,241));
        c.rect(viewX,viewY,viewW,39,surface);
        c.rect(viewX,viewY+38,viewW,1,rule);
        if(benchSetup_) {
            c.text(viewX+17,viewY+8,"USB PROFILE  /  EULOBENCH/2 DEVICES ONLY",ink,0,678);
            c.text(viewX+17,viewY+56,"Select your board's own VID:PID and verified vendor bulk interface.",ink,0,676);
            c.text(viewX+17,viewY+88,"00 endpoints = detect from descriptor; other values must match exactly.",muted,0,676);
            c.rect(viewX+17,viewY+123,viewW-34,1,rule);
            c.text(viewX+17,viewY+137,bench_ && bench_->connected()?
                "BOARD-APPROVED PWM GPIO POOL  /  READ ONLY":
                benchCapabilitiesSeen_?"LAST IDENTIFIED PIN POOL  /  RE-IDENTIFY TO APPLY":
                                       "BOARD-APPROVED PWM GPIO POOL  /  READ ONLY",muted);
            if((bench_ && bench_->connected()) || benchCapabilitiesSeen_) {
                const auto caps=bench_ && bench_->connected()?
                    bench_->capabilities():lastBenchCapabilities_;
                std::string allowed[2];
                for(int i=0;i<8;++i)if(caps.allowed[i]!=bench::unsetPin) {
                    std::string& row=allowed[i/4];
                    if(!row.empty())row+="   /   ";
                    row+=bench::pinName(static_cast<bench::Family>(caps.family),
                                        caps.allowed[i]);
                }
                c.text(viewX+17,viewY+163,allowed[0],ink,0,678);
                if(!allowed[1].empty())c.text(viewX+17,viewY+186,allowed[1],ink,0,678);
                c.text(viewX+17,viewY+212,
                    std::string(bench_ && bench_->connected()?"Fixed gate input: ":
                                                        "Last reported gate input: ")+
                    bench::pinName(static_cast<bench::Family>(caps.family),caps.gatePin),muted);
            } else {
                c.text(viewX+17,viewY+169,"Identify a board to inspect its allowed GPIOs.",ink);
                c.text(viewX+17,viewY+212,"The gate input cannot be remapped over USB.",muted);
            }
            c.rect(viewX+17,viewY+241,viewW-34,1,rule);
            c.text(viewX+17,viewY+256,"REQUESTED LOGICAL CHANNELS  /  NO HARDWARE CHANGE YET",muted,0,678);
            const std::string pitch=bench::pinName(benchProfile_.family,benchProfile_.pins[0]);
            const std::string yaw=bench::pinName(benchProfile_.family,benchProfile_.pins[1]);
            const std::string a=bench::pinName(benchProfile_.family,benchProfile_.pins[2]);
            const std::string b=bench::pinName(benchProfile_.family,benchProfile_.pins[3]);
            c.text(viewX+17,viewY+282,"Pitch: "+pitch+"     Yaw: "+yaw,ink,0,674);
            c.text(viewX+17,viewY+313,"Fin A: "+a+"     Fin B: "+b,ink,0,674);
            c.text(viewX+17,viewY+345,"Save USB IDs, identify, edit pins, save, re-identify, apply.",muted,0,675);
            sectionButton(c,270,486,300,"Identify / read GPIO pool",
                focusRegion_==5 && benchSetupFocus_==0,
                hoverX_>=270 && hoverX_<570 && hoverY_>=486 && hoverY_<519);
            sectionButton(c,580,486,354,"Apply pin map (gate OPEN)",
                focusRegion_==5 && benchSetupFocus_==1,
                hoverX_>=580 && hoverX_<934 && hoverY_>=486 && hoverY_<519);
            c.text(viewX+17,viewY+421,"No servo power from USB. Use an independent physical cut-off.",muted,0,675);
            c.border(viewX,viewY,viewW,viewH,rule);
        } else {
            c.text(viewX+17,viewY+8,"SERVO BENCH  /  MANUAL USB ONLY",ink);
            c.text(viewX+17,viewY+57,bench_ && bench_->connected()?
                bench_->physicalGate()?"Board identified. Gate reported enabled at handshake.":
                                        "Board identified. Physical enable gate open.":
                "Disconnected. No actuator commands are sent.",ink,0,670);
            c.text(viewX+17,viewY+89,"COMMAND CHANNEL / NOT MEASURED SERVO POSITION",muted);
            constexpr const char* labels[]={"TVC pitch","TVC yaw","Fin A","Fin B"};
            for(int i=0;i<4;++i) {
                const int y=viewY+132+i*63;
                if(i==benchAxis_) {
                    c.rect(viewX+14,y-6,viewW-28,52,accentPale);
                    c.rect(viewX+14,y-6,4,52,accent);
                } else c.rect(viewX+14,y-6,viewW-28,52,surface);
                c.text(viewX+36,y+4,labels[i],i==benchAxis_?ink:muted);
                c.line(viewX+280,y+17,viewX+626,y+17,rule,2);
                c.line(viewX+453,y+7,viewX+453,y+26,i==benchAxis_?accent:muted,2);
                c.text(viewX+275,y+24,"-2 deg",muted);
                c.text(viewX+599,y+24,"+2 deg",muted);
            }
            c.text(viewX+17,viewY+403,"ACK confirms receipt, not measured travel. No flight link.",muted,0,676);
            c.border(viewX,viewY,viewW,viewH,rule);
        }
    } else if(page_==Page::Ground) {
        if(recording_.samples.empty()) {
            c.rect(viewX,viewY,viewW,viewH,rgb(235,240,241));
            c.rect(viewX,viewY,viewW,39,surface);
            c.rect(viewX,viewY+38,viewW,1,rule);
            c.text(viewX+17,viewY+9,"GROUND / NO RECORDED DATA",ink);
            c.text(viewX+72,viewY+133,"Open a recorded sensor file",ink,1);
            c.rect(viewX+73,viewY+200,548,1,rule);
            c.text(viewX+73,viewY+225,"Place recording.csv beside project.eul.",ink);
            c.text(viewX+73,viewY+259,"File > Load recorded CSV, then Calibrate.",muted);
            c.text(viewX+73,viewY+319,"Servo bench is separate; no live sensor feed.",muted);
            c.border(viewX,viewY,viewW,viewH,rule);
        } else {
            const MassProperties mass=massProperties(rocket_,motor().loadedKg,motor().propellantKg);
            RocketView view;
            view.x=viewX;view.y=viewY;view.w=viewW;view.h=viewH;
            view.yaw+=yawOffset_;view.pitch+=pitchOffset_;view.zoom=zoom_;
            view.cameraPreset=cameraPreset_;
            drawRecordedView(c,rocket_,mass,view,recording_.at(groundTime_),
                             recording_.calibrated,recording_.altitudeZero);
        }
    } else {
        const MassProperties mass=massProperties(rocket_,motor().loadedKg,motor().propellantKg);
        RocketView view;
        view.x=viewX;view.y=viewY;view.w=viewW;view.h=viewH;
        view.yaw+=yawOffset_;view.pitch+=pitchOffset_;view.zoom=zoom_;
        view.cameraPreset=cameraPreset_;
        view.reference=reference_.triangles.empty()?nullptr:&reference_;
        view.flight=page_==Page::Flight;
        view.selected=page_==Page::Garage?selectedPart_:-1;
        if(view.flight) {
            view.sample=flight_.at(simTime_);
            view.trajectory=&flight_;
            view.altitude=view.sample.altitude;
            view.thrust=view.sample.thrust;
            view.canopyOpen=view.sample.canopyOpen;
        }
        drawRocketView(c,rocket_,mass,view);
    }
    drawChrome(c);
    drawTree(c);
    drawInspector(c);
    drawAnalysis(c);
    drawStatus(c);
    drawMenu(c);
    drawHelp(c);
    drawDiscardConfirmation(c);
    drawBenchConfirmation(c);
    drawBenchHexEditor(c);
}
} // namespace eulo
