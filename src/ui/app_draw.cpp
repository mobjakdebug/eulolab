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
    for(int i=0;i<10;++i) {
        const Tool& t=tools[i];
        const bool active=(t.action==Action::Garage && page_==Page::Garage) ||
            (t.action==Action::Flight && page_==Page::Flight) ||
            (t.action==Action::Ground && page_==Page::Ground);
        const bool focused=focusRegion_==1 && i==toolbarFocus_;
        const bool over=hoverX_>=t.x && hoverX_<t.x+t.w &&
            hoverY_>=toolbarY+5 && hoverY_<toolbarY+39;
        const bool disabled=(t.action==Action::Undo && !hasUndo_) ||
            ((t.action==Action::JumpApogee || t.action==Action::TogglePlay) && !flight_.valid);
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
    c.text(16,111,"Assembly",ink,1);
    c.text(17,144,"6 editable components",muted);
    c.rect(15,163,treeW-31,1,rule);
    for(int i=0;i<6;++i) {
        const int y=treeRowY+i*treeRowH;
        if(i==selectedPart_) {
            c.rect(0,y,treeW-1,treeRowH,accentPale);
            c.rect(0,y,4,treeRowH,accent);
        } else if(hoverX_>=0 && hoverX_<treeW && hoverY_>=y && hoverY_<y+treeRowH) {
            c.rect(0,y,treeW-1,treeRowH,rgb(234,240,242));
        }
        c.text(17,y+8,"0"+std::to_string(i+1),i==selectedPart_?accent:muted);
        c.text(53,y+2,partNames[i],i==selectedPart_?ink:dark);
        c.text(53,y+26,partNotes[i],muted);
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
        page_==Page::Flight?"Flight analysis":"USB diagnostics",ink,1);
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
            {"Idealised deployment","0 mm switches canopy off."}
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
            {"Velocity",formatted(state.velocity,1,"m/s")},
            {"Thrust",formatted(state.thrust,1,"N")},
            {"Proper load",formatted(state.properG,1,"g")},
            {"Dynamic q",formatted(state.dynamicPressure,0,"Pa")},
            {"Apogee",formatted(flight_.apogee,1,"m")},
            {"Touchdown",flight_.landed?formatted(flight_.touchdownSpeed,1,"m/s"):"Not reached"}
        };
        for(int i=0;i<8;++i)thinRow(c,inspectorX+6,fieldY+i*42,305,
                                     values[i].name,values[i].value,focusRegion_==3 && i==0);
        c.text(inspectorX+16,568,"1D point mass / no wind",ink);
        c.text(inspectorX+16,591,"Ideal canopy opens at apogee.",muted,0,286);
        sectionButton(c,inspectorX+16,635,288,"Edit components",focusRegion_==5,
                      hoverX_>=inspectorX+16 && hoverX_<1264 && hoverY_>=635 && hoverY_<668);
    } else {
        c.text(inspectorX+16,148,"Read-only / no telemetry decoder",muted);
        c.rect(inspectorX+16,171,288,1,rule);
        c.text(inspectorX+16,192,"Discovery status",muted);
        c.text(inspectorX+16,217,usbStatus_,ink,0,284);
        c.rect(inspectorX+16,250,288,1,rule);
        c.text(inspectorX+16,268,"Serial transfer",muted);
        c.text(inspectorX+16,293,"Not implemented",ink);
        c.rect(inspectorX+16,325,288,1,rule);
        c.text(inspectorX+16,343,"Incoming packets",muted);
        c.text(inspectorX+16,368,"No capture available",ink);
        c.rect(inspectorX+16,400,288,1,rule);
        c.text(inspectorX+16,568,"No live data is displayed.",ink);
        c.text(inspectorX+16,591,"Use Flight for calculated data.",muted);
        sectionButton(c,inspectorX+16,635,288,"Edit components",focusRegion_==5,
                      hoverX_>=inspectorX+16 && hoverX_<1264 && hoverY_>=635 && hoverY_<668);
    }
}

void App::drawAnalysis(Canvas& c) const {
    using namespace workbench;
    c.rect(viewX,analysisY,viewW,analysisH,surface);
    c.rect(viewX,analysisY,viewW,1,rule);
    if(page_==Page::Ground) {
        c.text(viewX+16,analysisY+8,"Data / USB receiver",ink);
        c.text(viewX+16,analysisY+44,"No recorded telemetry is available.",muted);
        c.text(viewX+16,analysisY+73,"This build only attempts device discovery.",muted);
        return;
    }
    c.text(viewX+16,analysisY+5,"Trajectory",ink);
    c.text(viewX+107,analysisY+5,"/ altitude vs time",muted);
    if(!flight_.valid || flight_.samples.empty()) {
        c.text(viewX+16,analysisY+53,"No model run. Adjust geometry or motor clearance.",ink);
        return;
    }
    const int x0=314,x1=919,y0=653,y1=600;
    const double end=flight_.samples.back().time;
    c.text(viewX+16,analysisY+38,formatted(flight_.apogee,0,"m"),accent);
    c.text(viewX+16,analysisY+79,"0 m",muted);
    c.line(x0,y1,x0,y0,rule);c.line(x0,y0,x1,y0,rule);
    const int apx=x0+static_cast<int>(std::clamp(flight_.apogeeTime/end,0.0,1.0)*(x1-x0));
    c.line(apx,y1,apx,y0,rgb(197,211,219));
    const int skip=std::max(1,static_cast<int>(flight_.samples.size()/550));
    int px=x0,py=y0;
    for(size_t i=static_cast<size_t>(skip);i<flight_.samples.size();i+=static_cast<size_t>(skip)) {
        const auto& s=flight_.samples[i];
        const int x=x0+static_cast<int>((s.time/end)*(x1-x0));
        const int y=y0-static_cast<int>((s.altitude/std::max(0.01,flight_.apogee))*(y0-y1));
        c.line(px,py,x,y,accent,2);px=x;py=y;
    }
    c.line(px,py,x1,y0,accent,2);
    const int cursor=x0+static_cast<int>(std::clamp(simTime_/end,0.0,1.0)*(x1-x0));
    const double altitude=flight_.at(simTime_).altitude;
    const int cursorY=y0-static_cast<int>((altitude/std::max(0.01,flight_.apogee))*(y0-y1));
    c.line(cursor,y1-2,cursor,y0+4,dark);
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
    c.text(697,statusY+6,"MODEL ONLY / NOT FLIGHT-RATED",muted);
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
        "Use +/- to step; drag in the view to orbit, wheel to zoom.",
        "Tab cycles focus; arrows navigate; Alt opens the menu.",
        "Ctrl+S saves; Ctrl+O opens; Ctrl+N resets; Ctrl+Z undoes.",
        "CONTROLLER  /  ORBIS BUILD",
        "L1/R1: part   |   L2/R2: focus   |   D-pad: select/step",
        "X: confirm/step   |   O: back   |   Square: save",
        "Options: menu   |   Triangle: USB   |   R3: fit view"
    };
    const char* limits[] = {
        "EDUCATIONAL VERTICAL POINT-MASS MODEL",
        "Published Klima C6/D9 curves are read-only motor data.",
        "Shell, fins and recovery use illustrative material values.",
        "Constant Cd values are assumptions, not wind-tunnel data.",
        "No wind, pitch, 6-DoF stability or structural analysis.",
        "Canopy deploys ideally at apogee without delay or failure.",
        "USB is discovery only; telemetry is not captured or decoded.",
        "Do not use results to certify a rocket or plan a flight."
    };
    const int count=helpMode_==1?9:8;
    for(int i=0;i<count;++i) {
        const char* line=helpMode_==1?controls[i]:limits[i];
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

void App::draw(Canvas& c,double /*now*/) const {
    using namespace workbench;
    c.clear(chrome);
    if(page_==Page::Ground) {
        c.rect(viewX,viewY,viewW,viewH,rgb(235,240,241));
        c.rect(viewX,viewY,viewW,39,surface);
        c.rect(viewX,viewY+38,viewW,1,rule);
        c.text(viewX+17,viewY+9,"USB / DISCOVERY ONLY",ink);
        c.text(viewX+72,viewY+133,"No live telemetry",ink,1);
        c.rect(viewX+73,viewY+200,548,1,rule);
        c.text(viewX+73,viewY+225,"No serial bytes are read by this build.",ink);
        c.text(viewX+73,viewY+259,"A discovered device is not a data connection.",muted);
        c.text(viewX+73,viewY+319,"See the inspector for discovery status.",muted);
        c.border(viewX,viewY,viewW,viewH,rule);
    } else {
        const MassProperties mass=massProperties(rocket_,motor().loadedKg,motor().propellantKg);
        RocketView view;
        view.x=viewX;view.y=viewY;view.w=viewW;view.h=viewH;
        view.yaw+=yawOffset_;view.pitch+=pitchOffset_;view.zoom=zoom_;
        view.flight=page_==Page::Flight;
        view.selected=page_==Page::Garage?selectedPart_:-1;
        if(view.flight) {
            const FlightSample s=flight_.at(simTime_);
            view.altitude=s.altitude;
            view.thrust=s.thrust;
            view.canopyOpen=rocket_.parachuteDiameter>0 &&
                s.time>flight_.apogeeTime && s.altitude>0;
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
}
} // namespace eulo
