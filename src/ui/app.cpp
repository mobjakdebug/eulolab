#include "app.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace eulo {
namespace {
const uint32_t base=rgb(8,15,22), panel=rgb(15,26,35), surface=rgb(21,35,44);
const uint32_t stroke=rgb(43,65,72), soft=rgb(101,129,139);
const uint32_t ink=rgb(231,236,228), orange=rgb(255,174,103), cyan=rgb(106,213,203);
const char* parts[] = {"BODY TUBE", "NOSE CONE", "FIN ASSEMBLY", "AVIONICS", "MOTOR DATA", "RECOVERY"};
const char* partCodes[] = {"01", "02", "03", "04", "05", "06"};
std::string fmt(const char* format,double value) {
    char b[80]; std::snprintf(b,sizeof(b),format,value); return b;
}
void smallLabel(Canvas& c,int x,int y,const std::string& text,uint32_t color=soft) {
    c.text(x,y,text,color,0);
}
void heading(Canvas& c,int x,int y,const std::string& text,uint32_t color=ink) {
    c.text(x,y,text,color,1);
}
void lineDivider(Canvas& c,int x,int y,int w) { c.rect(x,y,w,1,stroke); }
void badge(Canvas& c,int x,int y,const std::string& text,uint32_t fg,uint32_t bg,int w) {
    c.roundRect(x,y,w,30,6,bg);
    c.text(x+10,y+4,text,fg,0,w-18);
}
void field(Canvas& c,int x,int y,int w,const std::string& label,
           const std::string& val,bool selected) {
    if(selected) { c.roundRect(x,y,w,63,7,rgb(40,47,48));c.rect(x,y+7,3,49,orange); }
    else c.roundRect(x,y,w,63,7,surface);
    smallLabel(c,x+15,y+5,label,selected?orange:soft);
    c.text(x+15,y+30,val,ink,0,w-(selected?73:28));
    if(selected) c.text(x+w-47,y+31,"<>",orange);
}
}

bool App::init(const char* fontPath,const char* motorDirectory,const char* projectPath) {
    if(!font_.load(fontPath) || !motorDirectory) return false;
    projectPath_ = projectPath?projectPath:"project.ini";
    std::string error;
    const std::string directory(motorDirectory);
    if(!motors_[0].load((directory+"/Klima_C6.eng").c_str(),error) ||
       !motors_[1].load((directory+"/Klima_D9.eng").c_str(),error)) {
        status_=error;return false;
    }
    rebuild();
    return true;
}
void App::rebuild() { flight_=simulate(rocket_,motor()); }
int App::fieldCount() const { return selectedPart_==0?4:1; }
void App::editField(int dir) {
    const Rocket previous = rocket_;
    switch(selectedPart_) {
        case 0:
            if(selectedField_==0) rocket_.bodyLength=std::clamp(rocket_.bodyLength+dir*.01,.25,2.0);
            else if(selectedField_==1) rocket_.radius=std::clamp(rocket_.radius+dir*.001,.012,.09);
            else if(selectedField_==2) rocket_.wall=std::clamp(rocket_.wall+dir*.0002,.0005,rocket_.radius*.4);
            else {
                int m=(static_cast<int>(rocket_.material)+dir+3)%3;
                rocket_.material=static_cast<Material>(m);
                rocket_.bodyDensity=illustrativeDensity(rocket_.material);
            }
            break;
        case 1: rocket_.noseLength=std::clamp(rocket_.noseLength+dir*.01,.06,.45); break;
        case 2: rocket_.finSpan=std::clamp(rocket_.finSpan+dir*.005,.02,.25); break;
        case 3: rocket_.electronicsMass=std::clamp(rocket_.electronicsMass+dir*.005,.0,.75); break;
        case 4: rocket_.motorIndex=(rocket_.motorIndex+dir+2)%2;break;
        case 5:
            if(rocket_.parachuteDiameter==0 && dir>0) rocket_.parachuteDiameter=.20;
            else if(rocket_.parachuteDiameter<=.10 && dir<0) rocket_.parachuteDiameter=0;
            else if(rocket_.parachuteDiameter>0)
                rocket_.parachuteDiameter=std::clamp(rocket_.parachuteDiameter+dir*.02,.10,1.5);
            break;
        default: return;
    }
    if(!rocket_.valid() || motor().diameterMm*.0005>rocket_.radius-rocket_.wall) {
        rocket_=previous; status_="Invalid geometry or motor clearance."; return;
    }
    rebuild();
    status_="Model updated - estimates recalculated.";
}
void App::action(Action a) {
    demoMode_=false;
    switch(a) {
        case Action::NewProject: rocket_=Rocket{}; rebuild(); page_=Page::Garage; selectedPart_=0; selectedField_=0; simTime_=0; playing_=false; status_="New project created."; break;
        case Action::Garage: if(page_!=Page::Home) page_=Page::Garage; break;
        case Action::Flight:
            if(page_!=Page::Home && flight_.valid) {page_=Page::Flight;simTime_=0;playing_=true;}
            else status_="Simulation unavailable - check the motor fit.";
            break;
        case Action::Ground: if(page_!=Page::Home) {page_=Page::Ground;playing_=false;} break;
        case Action::Back: page_=page_==Page::Flight||page_==Page::Ground?Page::Garage:Page::Home;playing_=false;break;
        case Action::NextPart: selectedPart_=(selectedPart_+1)%6;selectedField_=0;break;
        case Action::PrevPart: selectedPart_=(selectedPart_+5)%6;selectedField_=0;break;
        case Action::NextField: selectedField_=(selectedField_+1)%fieldCount();break;
        case Action::PrevField: selectedField_=(selectedField_+fieldCount()-1)%fieldCount();break;
        case Action::Increase: if(page_==Page::Garage) editField(1);
            else if(page_==Page::Flight && !flight_.samples.empty())
                simTime_=std::min(flight_.samples.back().time,simTime_+.5);
            break;
        case Action::Decrease: if(page_==Page::Garage) editField(-1);
            else if(page_==Page::Flight) simTime_=std::max(0.,simTime_-.5);
            break;
        case Action::TogglePlay: if(page_==Page::Flight) playing_=!playing_; break;
        case Action::Save: status_=saveProject(rocket_,projectPath_.c_str())?"Project saved.":"Save failed - check storage access.";break;
        case Action::Load:
            if(loadProject(rocket_,projectPath_.c_str())) {
                rebuild();status_=flight_.valid?"Project loaded.":"Motor does not fit tube.";
                page_=Page::Garage;playing_=false;
            } else status_="No valid saved project.";
            break;
    }
}
void App::tick(double seconds) {
    if(!playing_||!flight_.valid||flight_.samples.empty())return;
    simTime_+=std::clamp(seconds,0.0,0.1);
    if(simTime_>flight_.samples.back().time) {simTime_=flight_.samples.back().time;playing_=false;}
}
void App::demoAt(double seconds) {
    // Deterministic HOST recording of the actual App actions. Replaying from
    // a fresh project makes standalone screenshots agree with the full video.
    const int stage=seconds<4?0:seconds<10?1:seconds<16?2:seconds<22?3:
        seconds<28?4:seconds<34?5:seconds<40?6:seconds<45?7:
        seconds<73?8:9;
    if(stage!=demoStage_) {
        demoStage_=stage;
        if(stage==0) {page_=Page::Home;status_="Create a new project.";}
        else {
            action(Action::NewProject);
            if(stage>=2) {
                selectedPart_=0;selectedField_=3;
                action(Action::Increase);action(Action::Increase); // carbon laminate
            }
            if(stage>=3) {
                selectedPart_=1;selectedField_=0;
                action(Action::Increase);action(Action::Increase); // nose length
            }
            if(stage>=4) {
                selectedPart_=2;action(Action::Increase);action(Action::Increase); // fins
            }
            if(stage>=5) { selectedPart_=3;action(Action::Increase); } // avionics
            if(stage>=6) { selectedPart_=4;action(Action::Increase); } // Klima D9
            if(stage>=7) {
                selectedPart_=5;
                action(Action::Increase);action(Action::Increase);action(Action::Increase);
            }
            if(stage<=7) {
                selectedPart_=stage==3?1:stage==4?2:stage==5?3:stage==6?4:stage==7?5:0;
                selectedField_=stage==2?3:0;
                status_=stage==1?"01 / 25 mm body, 34 cm tube":
                    stage==2?"02 / choose illustrative laminate":
                    stage==3?"03 / extend the nose profile":
                    stage==4?"04 / adjust three balsa fins":
                    stage==5?"05 / assign flight-logger mass":
                    stage==6?"06 / select published Klima D9 data":
                    "07 / ideal canopy recovery estimate";
            } else if(stage==8) {
                action(Action::Flight);
                playing_=false;
                status_="08 / time-compressed simulation, not a real launch";
            } else {
                action(Action::Ground);
                status_="09 / PS4 USB discovery only; no live link";
            }
        }
    }
    demoMode_=true;
    if(stage==8 && !flight_.samples.empty()) {
        // Compress the two phases independently so the full ascent and
        // idealised descent are visible in a short, labelled HOST replay.
        const double u=std::clamp((seconds-45)/28.0,0.0,1.0);
        const double end=flight_.samples.back().time;
        simTime_=u<.4?flight_.apogeeTime*u/.4:
            flight_.apogeeTime+(end-flight_.apogeeTime)*(u-.4)/.6;
    }
}

void App::drawChrome(Canvas& c) const {
    c.clear(base);
    c.rect(0,0,c.width(),79,rgb(11,20,27));
    c.rect(0,77,c.width(),1,stroke);
    c.roundRect(21,17,44,44,8,orange);
    c.rect(34,26,5,25,base);
    c.rect(34,26,19,5,base);
    c.rect(34,37,14,4,base);
    c.rect(34,47,19,4,base);
    heading(c,79,8,"EULO",ink);
    heading(c,191,8,"LAB",orange);
    smallLabel(c,82,46,"NATIVE ROCKET DESIGN STUDIO",soft);
    c.line(307,18,307,57,stroke);
    smallLabel(c,334,27,"PROJECT /",soft);
    smallLabel(c,438,27,page_==Page::Home?"UNTITLED": "AURORA - 01",ink);
    badge(c,1072,24,demoMode_?"HOST REPLAY":"PS4 NATIVE",demoMode_?orange:cyan,
          demoMode_?rgb(61,42,32):rgb(25,52,53),165);
}

void App::drawSidebar(Canvas& c) const {
    const int x=20,y=102,w=238,h=482;
    c.roundRect(x,y,w,h,9,panel);
    c.border(x,y,w,h,stroke);
    smallLabel(c,x+19,y+15,page_==Page::Home?"NAVIGATION":
        page_==Page::Garage?"ASSEMBLY TREE":page_==Page::Flight?"MISSION TIMELINE":"GROUND LINK",orange);
    lineDivider(c,x+18,y+45,w-36);
    if(page_==Page::Home) {
        const char* items[]={"NEW PROJECT", "ASSEMBLY GARAGE", "FLIGHT SIMULATION", "GROUND STATION"};
        for(int i=0;i<4;++i) {
            const int ry=y+65+i*71;
            c.roundRect(x+12,ry,w-24,54,6,i==0?rgb(53,54,46):surface);
            smallLabel(c,x+27,ry+13,items[i],i==0?orange:soft);
        }
        smallLabel(c,x+20,y+384,"ALL MODULES / LOCAL",soft);
        smallLabel(c,x+20,y+410,"NO CLOUD REQUIRED",cyan);
    } else if(page_==Page::Garage) {
        for(int i=0;i<6;++i) {
            const int ry=y+58+i*62;
            const bool on=i==selectedPart_;
            c.roundRect(x+11,ry,w-22,53,6,on?rgb(54,52,44):surface);
            if(on)c.rect(x+11,ry+9,3,35,orange);
            smallLabel(c,x+26,ry+11,partCodes[i],on?orange:soft);
            smallLabel(c,x+62,ry+11,parts[i],on?ink:soft);
        }
    } else {
        const char* labels[]= {"MISSION PROFILE", "LAUNCH PAD", "ASCENT", "APOGEE", "DESCENT", "TELEMETRY"};
        for(int i=0;i<6;++i) {
            const int ry=y+59+i*62;
            const bool active=page_==Page::Flight?(i<=3 || (i==4 && simTime_>flight_.apogeeTime)):(i==5);
            c.circle(x+30,ry+20,4,active?cyan:stroke);
            smallLabel(c,x+48,ry+9,labels[i],active?ink:soft);
            if(i!=5)c.rect(x+29,ry+31,2,30,stroke);
        }
    }
    c.roundRect(x+12,y+h-42,w-24,28,5,surface);
    smallLabel(c,x+21,y+h-41,page_==Page::Ground?"USB / READ ONLY":"LOCAL PROJECT",cyan);
}

void App::drawHome(Canvas& c) const {
    c.gradient(276,102,624,482,rgb(17,43,53),rgb(10,23,35));
    for(int i=0;i<19;++i) {
        c.line(276+24+i*33,103,276+24+i*33,584,rgb(27,53,63));
    }
    c.circle(590,311,173,rgb(22,47,56));
    c.ring(590,311,151,1,rgb(64,97,99));
    c.ring(590,311,127,1,rgb(53,85,90));
    c.line(590,132,590,492,rgb(53,83,85));
    c.line(407,311,773,311,rgb(53,83,85));
    smallLabel(c,318,132,"OPEN AN IDEA. MODEL THE FLIGHT.",cyan);
    heading(c,322,211,"BUILD THE",ink);
    heading(c,322,269,"POSSIBLE.",orange);
    smallLabel(c,326,346,"A native PS4 workspace for model-rocket design,",ink);
    smallLabel(c,326,372,"flight estimates and read-only ground telemetry.",ink);
    c.roundRect(323,427,237,52,8,orange);
    smallLabel(c,343,440,"NEW PROJECT  [X]",base);
    c.border(276,102,624,482,stroke);
}

void App::drawInspector(Canvas& c,double now) const {
    (void)now;
    const int x=918,y=102,w=342,h=482;
    c.roundRect(x,y,w,h,9,panel);c.border(x,y,w,h,stroke);
    if(page_==Page::Home) {
        smallLabel(c,x+19,y+19,"THE WORKSPACE",orange);
        heading(c,x+16,y+60,"ONE SYSTEM.",ink);
        heading(c,x+16,y+111,"EVERY LAYER.",ink);
        lineDivider(c,x+19,y+181,w-38);
        const char* features[]={"01 / PARAMETRIC ASSEMBLY","02 / MATERIAL-AWARE MASS","03 / COMMERCIAL .ENG DATA","04 / 1D FLIGHT PROTOTYPE","05 / USB DISCOVERY"};
        for(int i=0;i<5;++i) smallLabel(c,x+20,y+201+i*39,features[i],i==3?orange:ink);
        c.roundRect(x+18,y+427,w-36,37,6,rgb(49,45,36));
        smallLabel(c,x+29,y+433,"EARLY BUILD  /  NOT FLIGHT RATED",orange);
        return;
    }
    if(page_==Page::Garage) {
        smallLabel(c,x+18,y+16,"COMPONENT INSPECTOR / "+std::string(partCodes[selectedPart_]),orange);
        heading(c,x+17,y+48,parts[selectedPart_],ink);
        lineDivider(c,x+18,y+106,w-36);
        int fy=y+121;
        if(selectedPart_==0) {
            field(c,x+15,fy,w-30,"BODY LENGTH",fmt("%.3f m",rocket_.bodyLength),selectedField_==0);fy+=69;
            field(c,x+15,fy,w-30,"OUTER DIAMETER",fmt("%.1f mm",rocket_.radius*2000),selectedField_==1);fy+=69;
            field(c,x+15,fy,w-30,"WALL THICKNESS",fmt("%.1f mm",rocket_.wall*1000),selectedField_==2);fy+=69;
            field(c,x+15,fy,w-30,"MATERIAL / DENSITY",materialName(rocket_.material),selectedField_==3);
        } else if(selectedPart_==1) {
            field(c,x+15,fy,w-30,"CONE LENGTH",fmt("%.3f m",rocket_.noseLength),true);fy+=69;
            field(c,x+15,fy,w-30,"CONE PROFILE","Conic / analytical mesh",false);fy+=85;
            smallLabel(c,x+18,fy,"No strength test in model.",soft);
        } else if(selectedPart_==2) {
            field(c,x+15,fy,w-30,"FIN SPAN",fmt("%.1f mm",rocket_.finSpan*1000),true);fy+=69;
            field(c,x+15,fy,w-30,"FIN COUNT","3 radial / illustrative balsa",false);fy+=85;
            smallLabel(c,x+18,fy,"Fin mass is an illustrative estimate.",soft);
        } else if(selectedPart_==3) {
            field(c,x+15,fy,w-30,"AVIONICS MASS",fmt("%.3f kg",rocket_.electronicsMass),true);fy+=69;
            field(c,x+15,fy,w-30,"PAYLOAD MODEL","Mass-only / no board I/O",false);fy+=85;
            smallLabel(c,x+18,fy,"Mass changes the computed CG.",soft);
        } else if(selectedPart_==4) {
            field(c,x+15,fy,w-30,"PUBLISHED .ENG MOTOR",
                motor().manufacturer+" "+motor().name,true);fy+=69;
            field(c,x+15,fy,w-30,"TOTAL IMPULSE",fmt("%.1f N*s",motor().totalImpulse()),false);fy+=69;
            field(c,x+15,fy,w-30,"BURN TIME",fmt("%.2f s",motor().burnTime()),false);fy+=75;
            smallLabel(c,x+18,fy,"PD data / Leo Nutz.",soft);
        } else {
            field(c,x+15,fy,w-30,"CANOPY DIAMETER",rocket_.parachuteDiameter>0?
                fmt("%.2f m / ideal",rocket_.parachuteDiameter):"Disabled",true);fy+=69;
            field(c,x+15,fy,w-30,"DEPLOYMENT MODEL","Instant at apogee",false);fy+=80;
            smallLabel(c,x+18,fy,"No opening shock model.",orange);
        }
        lineDivider(c,x+18,y+415,w-36);
        smallLabel(c,x+20,y+425,"MASS / ILLUSTRATIVE",soft);
        const auto mass=massProperties(rocket_,motor().loadedKg,motor().propellantKg);
        c.text(x+20,y+451,fmt("%.3f kg LOADED",mass.loaded),cyan);
    } else if(page_==Page::Flight) {
        const FlightSample s=flight_.at(simTime_);
        smallLabel(c,x+18,y+16,"FLIGHT ANALYSIS / 1D MODEL",orange);
        heading(c,x+16,y+48,s.altitude<=0 && simTime_>0?"LANDED":
            s.velocity<0?"DESCENT":"ASCENT",ink);
        lineDivider(c,x+18,y+106,w-36);
        field(c,x+15,y+119,w-30,"ALTITUDE",fmt("%.1f m",s.altitude),false);
        field(c,x+15,y+190,w-30,"VELOCITY",fmt("%.1f m/s",s.velocity),false);
        field(c,x+15,y+261,w-30,"MOTOR THRUST / .ENG",fmt("%.1f N",s.thrust),false);
        smallLabel(c,x+20,y+350,"MODEL APOGEE",soft);
        heading(c,x+19,y+369,fmt("%.0f m",flight_.apogee),cyan);
        smallLabel(c,x+20,y+448,rocket_.parachuteDiameter>0?
            "IDEAL CANOPY / NO WIND":"NO CANOPY / NO WIND",orange);
    } else {
        smallLabel(c,x+18,y+16,"GROUND STATION / USB",orange);
        heading(c,x+16,y+48,"TELEMETRY",ink);
        lineDivider(c,x+18,y+106,w-36);
        c.roundRect(x+15,y+130,w-30,84,7,rgb(38,40,40));
        smallLabel(c,x+30,y+141,"LINK STATUS",orange);
        smallLabel(c,x+30,y+173,usbStatus_,ink);
        smallLabel(c,x+18,y+246,"DATA SOURCE",soft);
        smallLabel(c,x+18,y+276,"No device is sending telemetry.",ink);
        smallLabel(c,x+18,y+310,"Simulation is never displayed",soft);
        smallLabel(c,x+18,y+335,"as measured telemetry.",soft);
        c.roundRect(x+15,y+417,w-30,45,7,rgb(25,49,49));
        smallLabel(c,x+31,y+428,"DISCOVERY ONLY / NO WRITE",cyan);
    }
}

void App::drawFooter(Canvas& c) const {
    c.roundRect(20,600,1240,101,9,panel);c.border(20,600,1240,101,stroke);
    smallLabel(c,36,607,page_==Page::Flight?"TRAJECTORY / ALTITUDE VS. TIME":"SYSTEM STATUS / DESIGN ITERATION",orange);
    if(page_==Page::Flight && flight_.valid && !flight_.samples.empty()) {
        const double tmax=flight_.samples.back().time;
        const double hmax=std::max(1.0,flight_.apogee);
        int px=369,py=663;
        for(int i=0;i<=220;++i) {
            const double t=tmax*i/220.0;
            const int cx=369+i*3;
            const int cy=663-static_cast<int>(flight_.at(t).altitude/hmax*33);
            if(i)c.line(px,py,cx,cy,cyan,2);
            px=cx;py=cy;
        }
        const int cur=369+static_cast<int>(std::clamp(simTime_/tmax,0.0,1.0)*660);
        c.line(cur,625,cur,667,orange);
        smallLabel(c,37,642,fmt("TIME  %.1f s",simTime_),ink);
        smallLabel(c,1062,647,fmt("MAX G  %.1f",flight_.maxG),ink);
    } else {
        smallLabel(c,37,648,status_,ink);
        if(page_==Page::Garage) {
            const auto mass=massProperties(rocket_,motor().loadedKg,motor().propellantKg);
            smallLabel(c,680,638,fmt("CG   %.3f m",mass.cgFromTail),cyan);
            smallLabel(c,997,638,fmt("DRY   %.3f kg",mass.dry),soft);
        }
    }
    c.rect(20,676,1240,1,stroke);
    const char* instructions=page_==Page::Home?"X  NEW PROJECT     O  BACK":
        page_==Page::Garage?"L1/R1  PART     UP/DOWN  FIELD     LEFT/RIGHT  EDIT     X  SIMULATE     []  SAVE     O  BACK":
        page_==Page::Flight?"X  PAUSE     LEFT/RIGHT  SCRUB     TRIANGLE  GROUND LINK     O  GARAGE":
        "TRIANGLE  GARAGE     USB DISCOVERY REQUIRES COMPATIBLE HARDWARE";
    smallLabel(c,36,675,instructions,soft);
}

void App::draw(Canvas& c,double now) const {
    drawChrome(c);
    if(page_==Page::Home) drawHome(c);
    else {
        const MassProperties mass=massProperties(rocket_,motor().loadedKg,motor().propellantKg);
        RocketView view;
        view.x=276;view.y=102;view.w=624;view.h=482;
        view.flight=page_==Page::Flight;
        const auto sample=flight_.at(simTime_);
        view.altitude=view.flight?sample.altitude:0;
        view.thrust=view.flight?sample.thrust:0;
        view.canopyOpen=view.flight && rocket_.parachuteDiameter>0 &&
            simTime_>flight_.apogeeTime && sample.altitude>0;
        view.yaw=0.60+std::sin(now*0.36)*0.18;
        view.selected=selectedPart_;
        drawRocketView(c,rocket_,mass,view);
        if(page_==Page::Garage) {
            badge(c,296,524,"MODEL / EDIT MODE",cyan,rgb(27,56,57),212);
        } else if(page_==Page::Ground) {
            badge(c,296,524,"NO LIVE FLIGHT DATA",orange,rgb(61,44,37),247);
        }
    }
    drawSidebar(c);
    drawInspector(c,now);
    drawFooter(c);
}
} // namespace eulo
