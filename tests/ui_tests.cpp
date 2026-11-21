#include "ui/app.hpp"
#include "ui/workbench_layout.hpp"
#include "core/rocket.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <string>
#include <vector>

using namespace eulo;
namespace {
constexpr int w=1280,h=720;
constexpr uint32_t guard=0x12345678;
constexpr int counts[]={6,5,9,2,6,4,7,8,5};
using Frame=std::vector<uint32_t>;
bool near(double a,double b,double tolerance=1e-7) {return std::abs(a-b)<tolerance;}
Frame pixels() {return Frame(static_cast<size_t>(w)*h+2,guard);}
void render(const App& app,Canvas& canvas,const Frame& data) {
    app.draw(canvas,0);
    assert(data.front()==guard && data.back()==guard);
    assert(data[1+static_cast<size_t>(300)*w+600]!=guard);
}
void select(App& app,int part,int fieldIndex) {
    app.click(58,workbench::treeRowY+part*workbench::treeRowH+12);
    app.click(1024,workbench::fieldY+fieldIndex*workbench::fieldH+3);
    assert(app.page()==Page::Garage && app.selectedPart()==part &&
           app.selectedField()==fieldIndex);
}
double get(const Rocket& r,int part,int fieldIndex) {
    switch(part) {
        case 0:switch(fieldIndex) {
            case 0:return r.bodyLength*1000;case 1:return r.radius*2000;
            case 2:return r.wall*1000;case 3:return static_cast<int>(r.material);
            case 4:return r.bodyDensity;case 5:return r.airframeCd;
        }break;
        case 1:switch(fieldIndex) {
            case 0:return r.noseLength*1000;case 1:return static_cast<int>(r.noseProfile);
            case 2:return r.noseWall*1000;case 3:return static_cast<int>(r.noseMaterial);
            case 4:return r.noseDensity;
        }break;
        case 2:switch(fieldIndex) {
            case 0:return r.finCount;case 1:return r.finSpan*1000;
            case 2:return r.finRootChord*1000;case 3:return r.finTipChord*1000;
            case 4:return r.finSweep*1000;case 5:return r.finThickness*1000;
            case 6:return r.finSetback*1000;case 7:return static_cast<int>(r.finMaterial);
            case 8:return r.finDensity;
        }break;
        case 3:return fieldIndex==0?r.electronicsMass*1000:r.electronicsFromTail*1000;
        case 4:switch(fieldIndex) {
            case 0:return r.motorIndex;case 1:return r.motorFromTail*1000;
            case 2:return r.mountLength*1000;case 3:return r.mountBoreRadius*2000;
            case 4:return r.mountWall*1000;case 5:return r.mountDensity;
        }break;
        case 5:switch(fieldIndex) {
            case 0:return r.parachuteDiameter*1000;case 1:return r.canopyCd;
            case 2:return r.canopyArealDensity*1000;case 3:return r.recoveryFromTail*1000;
        }break;
        case 6:switch(fieldIndex) {
            case 0:return r.insertEnabled?1:0;case 1:return r.insertLength*1000;
            case 2:return r.insertOuterRadius*2000;case 3:return r.insertBoreRadius*2000;
            case 4:return r.insertFromTail*1000;
            case 5:return static_cast<int>(r.insertMaterial);
            case 6:return r.insertDensity;
        }break;
        case 7:switch(fieldIndex) {
            case 0:return r.launchTiltDeg;case 1:return r.launchHeadingDeg;
            case 2:return r.windEast;case 3:return r.windNorth;
            case 4:return r.gimbalPitchDeg;case 5:return r.gimbalYawDeg;
            case 6:return r.cpFromTail*1000;case 7:return r.angularDamping;
        }break;
        case 8:switch(fieldIndex) {
            case 0:return r.referenceFormat;case 1:return r.referenceUnitMm;
            case 2:return r.referenceEastMm;case 3:return r.referenceTailMm;
            case 4:return r.referenceNorthMm;
        }break;
    }
    assert(false);return 0;
}
void menu(App& app,int menuIndex,int item) {
    const auto& m=workbench::menus[menuIndex];
    app.click(m.x+15,workbench::menuY+6);
    assert(app.menuOpen());
    app.click(m.x+15,62+item*36+9);
    assert(!app.menuOpen());
}
void testEveryProperty() {
    // Each inspector control changes the same C++ model rendered in the viewport.
    std::filesystem::create_directories("out/field-test");
    FILE* mesh=std::fopen("out/field-test/reference.obj","wb");
    assert(mesh && std::fputs("v 0 0 0\nv 20 0 0\nv 0 20 0\nf 1 2 3\n",mesh)>0 &&
           std::fclose(mesh)==0);
    const std::array<std::vector<double>,9> alternatives = {{
        {365,27,1.0,1,1380,.75},
        {105,1,1.0,1,1380},
        {4,55,100,56,26,3,8,1,250},
        {25,235},
        {1,39,85,19.5,1.0,1350},
        {350,1.35,110,280},
        {1,40,14,4,230,1,1400},
        {4,90,2,-2,.5,-.5,120,.004},
        {1,1.5,12,16,-10}
    }};
    for(int p=0;p<9;++p) {
        assert(static_cast<int>(alternatives[p].size())==counts[p]);
        for(int i=0;i<counts[p];++i) {
            App app;
            assert(app.init("assets/font-atlas.bin","assets/motors","out/field-test/project.eul"));
            assert(app.fieldCount(p)==counts[p] && app.flight().valid);
            auto frame=pixels();Canvas canvas(frame.data()+1,w,h,app.font());
            select(app,p,i);
            render(app,canvas,frame);
            const Frame before=frame;
            const double old=get(app.rocket(),p,i);
            const double requested=alternatives[p][i];
            assert(!near(requested,old));
            if(!app.setSelectedValue(requested)) {
                std::fprintf(stderr,"Field rejected: part %d field %d: %s\n",
                             p,i,app.status().c_str());
                std::abort();
            }
            assert(near(get(app.rocket(),p,i),requested));
            assert(app.rocket().valid() && app.flight().valid);
            render(app,canvas,frame);
            assert(before!=frame); // actual pixels reflect the model change
            assert(!app.setSelectedValue(std::numeric_limits<double>::infinity()));
            assert(!app.setSelectedValue(-99999));
            assert(near(get(app.rocket(),p,i),requested));
        }
    }
    App stepped;
    assert(stepped.init("assets/font-atlas.bin","assets/motors","out/field-test/project.eul"));
    select(stepped,5,0);
    assert(stepped.setSelectedValue(105));
    stepped.action(Action::Decrease);
    assert(near(stepped.rocket().parachuteDiameter,.1));
    stepped.action(Action::Decrease);
    assert(near(stepped.rocket().parachuteDiameter,0));
    stepped.action(Action::Increase);
    assert(near(stepped.rocket().parachuteDiameter,.1));
    select(stepped,0,1);
    assert(!stepped.setSelectedValue(20)); // internal sleeve does not fit a 20 mm tube
    assert(stepped.status().find("dependent geometry")!=std::string::npos);
    std::remove("out/field-test/reference.obj");
    std::puts("PASS inspector: 52/52 editable properties, live model/render, limits and motor fit");
}
void testFocusNavigation() {
    App app;
    assert(app.init("assets/font-atlas.bin","assets/motors","out/focus-test.eul"));
    const Rocket original=app.rocket();
    for(int i=0;i<3;++i)app.key("Tab"); // inspector -> timeline -> button -> menu
    app.key("ArrowRight"); // change selected top-level menu, not a property
    assert(near(app.rocket().bodyLength,original.bodyLength));
    app.key("ArrowDown");assert(app.menuOpen());
    app.key("Escape");assert(!app.menuOpen());
    for(int i=0;i<4;++i)app.key("Tab"); // toolbar -> tree -> inspector -> timeline
    app.key("ArrowRight");
    assert(app.page()==Page::Flight && app.status().find("cursor")!=std::string::npos);
    assert(near(app.rocket().bodyLength,original.bodyLength));
    app.key("Tab"); // focus bottom inspector button
    app.key("ArrowRight");
    assert(near(app.rocket().bodyLength,original.bodyLength));
    app.key("Enter");assert(app.page()==Page::Garage);
    for(int i=0;i<5;++i)app.key("Tab"); // inspector -> tree through the focus cycle
    app.key("Enter"); // activating the tree moves focus to editable properties
    app.key("ArrowRight");
    assert(near(app.rocket().bodyLength,original.bodyLength+.01));
    std::puts("PASS focus: menu/timeline/button navigation never mutates an unrelated property");
}
void testCommandsAndInput() {
    const char* file="out/ui-project.eul";
    std::remove(file);
    App app;
    assert(app.init("assets/font-atlas.bin","assets/motors",file));
    auto frame=pixels();Canvas canvas(frame.data()+1,w,h,app.font());
    render(app,canvas,frame);
    const Frame noHover=frame;
    app.hover(83,workbench::menuY+8);render(app,canvas,frame);
    assert(noHover!=frame);
    app.hover(-1,-1);render(app,canvas,frame);
    assert(noHover==frame);
    menu(app,0,1); // File > Open, empty state
    assert(app.status().find("Open failed")!=std::string::npos && app.page()==Page::Garage);
    app.click(workbench::menus[0].x+8,workbench::menuY+8);
    assert(app.menuOpen());
    app.click(workbench::menus[0].x+8,workbench::menuY+8);
    assert(!app.menuOpen());
    // Host keyboard text entry, invalid input state and cancel.
    app.click(1100,workbench::fieldY+27);
    assert(app.editing());
    for(char ch:std::string("360"))app.key(std::string(1,ch));
    app.key("Enter");
    assert(!app.editing() && near(app.rocket().bodyLength,.36));
    app.click(1100,workbench::fieldY+27);
    for(char ch:std::string("4000"))app.key(std::string(1,ch));
    app.key("Enter");
    assert(app.editing() && app.status().find("enter")!=std::string::npos);
    app.wheel(1190,230,-1); // invalid value may not migrate to another field
    assert(app.editing() && app.selectedField()==0);
    app.key("Escape");
    assert(!app.editing() && near(app.rocket().bodyLength,.36));
    app.click(1250,workbench::fieldY+27);
    assert(near(app.rocket().bodyLength,.37));
    app.click(990,workbench::fieldY+27);
    assert(near(app.rocket().bodyLength,.36));
    app.key("z",true);
    assert(near(app.rocket().bodyLength,.37)); // undo last step
    // A click on a choice selector cycles the real NoseProfile enum.
    select(app,1,1);
    app.click(1160,workbench::fieldY+workbench::fieldH+28);
    assert(app.rocket().noseProfile==NoseProfile::TangentOgive);
    app.click(990,workbench::fieldY+workbench::fieldH+28);
    assert(app.rocket().noseProfile==NoseProfile::Cone);
    app.key("s",true);
    Rocket saved;
    assert(loadProject(saved,file) && near(saved.bodyLength,.37));
    // Explicit unsaved-change guard; default keyboard Enter cancels deletion.
    assert(app.setSelectedValue(1)); // nose profile
    app.key("n",true);
    assert(app.discardConfirmationOpen());
    const Rocket uncommitted=app.rocket();
    render(app,canvas,frame);
    app.key("Enter");
    assert(!app.discardConfirmationOpen() && app.rocket().noseProfile==uncommitted.noseProfile);
    app.key("n",true);
    assert(app.discardConfirmationOpen());
    app.click(529,433); // Save and continue (writes, then starts a new project)
    assert(!app.discardConfirmationOpen() && app.page()==Page::Garage &&
           app.rocket().noseProfile==Rocket{}.noseProfile);
    assert(loadProject(saved,file) && saved.noseProfile==NoseProfile::TangentOgive);
    // Reload, make an edit, then discard via the mouse before reopening.
    app.key("o",true);
    assert(app.rocket().noseProfile==NoseProfile::TangentOgive);
    select(app,1,1);
    assert(app.setSelectedValue(0)); // conical again
    app.key("o",true);
    assert(app.discardConfirmationOpen());
    app.click(777,433); // Discard changes
    assert(!app.discardConfirmationOpen() && app.rocket().noseProfile==NoseProfile::TangentOgive);
    select(app,1,1);
    app.click(1000,648); // Reset component button is a real model action
    assert(app.rocket().noseProfile==NoseProfile::Cone);
    // Clicking the timeline from Design enters Flight at the chosen time.
    app.click(610,631);
    assert(app.page()==Page::Flight);
    render(app,canvas,frame);
    const Frame midFlight=frame;
    app.action(Action::TogglePlay);
    app.tick(.1);
    render(app,canvas,frame);
    assert(midFlight!=frame);
    app.action(Action::JumpApogee);
    assert(app.page()==Page::Flight && app.status().find("apogee")!=std::string::npos);
    app.click(1000,649); // right-side Edit components button
    assert(app.page()==Page::Garage);
    app.setUsbStatus("USB test: no serial packets");
    app.action(Action::Ground);
    render(app,canvas,frame);
    assert(app.page()==Page::Ground);
    app.action(Action::Back);
    assert(app.page()==Page::Garage);
    // Zoom, orbit, fit; scissor prevents a large model bleeding over toolbar.
    const uint32_t toolbarPixel=frame[1+80*w+593];
    for(int n=0;n<8;++n)app.wheel(530,256,1);
    app.orbit(.7,.16);
    render(app,canvas,frame);
    assert(frame[1+80*w+593]==toolbarPixel);
    app.action(Action::FitView);
    render(app,canvas,frame);
    // Controller-only configuration must step, never open an unusable textbox.
    app.setTextEntryAvailable(false);
    select(app,4,2);
    const double previous=app.rocket().mountLength;
    app.action(Action::Confirm);
    assert(!app.editing() && app.rocket().mountLength>previous);
    // Menu keyboard navigation and focus, not just pointer clicks.
    app.action(Action::Menu);
    assert(app.menuOpen());
    app.key("ArrowRight");app.key("ArrowRight");
    app.key("ArrowDown");app.key("Enter"); // View > Flight analysis
    assert(app.page()==Page::Flight && !app.menuOpen());
    app.key("Escape");
    assert(app.page()==Page::Garage);
    app.key("F1");render(app,canvas,frame);
    app.click(855,180); // actual close button bounds
    app.action(Action::HelpModel);render(app,canvas,frame);app.key("Escape");
    // All menu actions are reachable by hitbox and return to a stable state.
    menu(app,0,2);assert(loadProject(saved,file)); // Save
    menu(app,1,0); // Undo or visible "nothing to undo" message
    menu(app,1,1); // Reset selected component
    menu(app,2,0);assert(app.page()==Page::Garage);
    menu(app,2,1);assert(app.page()==Page::Flight);
    menu(app,2,2);assert(app.page()==Page::Ground);
    menu(app,2,3);assert(app.page()==Page::Bench);
    app.click(1100,334);assert(app.status().find("No Orbis bench")!=std::string::npos);
    menu(app,2,4);assert(app.status().find("no camera")!=std::string::npos);
    app.action(Action::Garage);
    app.action(Action::FitView);assert(app.status().find("fitted")!=std::string::npos);
    menu(app,3,0);assert(app.page()==Page::Flight);
    menu(app,3,1);assert(app.page()==Page::Flight);
    menu(app,3,2);assert(app.page()==Page::Flight);
    menu(app,4,0);render(app,canvas,frame);app.action(Action::Back);
    menu(app,4,1);render(app,canvas,frame);app.action(Action::Back);
    app.action(Action::Garage);
    // Every toolbar control also uses the same actions, including failure state.
    for(const auto& tool:workbench::tools) {
        app.click(tool.x+tool.w/2,workbench::toolbarY+18);
        if(app.discardConfirmationOpen())app.action(Action::Back); // New/Open guard
        render(app,canvas,frame);
    }
    menu(app,0,0); // File > New project
    if(app.discardConfirmationOpen())app.click(777,433);
    assert(app.page()==Page::Garage && near(app.rocket().bodyLength,Rocket{}.bodyLength));
    std::remove(file);
    std::puts("PASS UI: pointer, keyboard, controller steps, menus, toolbar, undo, save/load, timeline, confirmation, help and USB states");
}
void testReferenceUI() {
    std::filesystem::create_directories("out/reference-ui");
    const char* file="out/reference-ui/project.eul";
    const char* mesh="out/reference-ui/reference.obj";
    FILE* f=std::fopen(mesh,"wb");
    assert(f && std::fputs("v -20 0 0\nv 20 0 0\nv 0 300 0\nf 1 2 3\n",f)>0 &&
           std::fclose(f)==0);
    App app;
    assert(app.init("assets/font-atlas.bin","assets/motors",file));
    auto frame=pixels();Canvas canvas(frame.data()+1,w,h,app.font());
    const double loaded=massProperties(app.rocket(),.0205,.0096).loaded;
    const double apogee=app.flight().apogee;
    render(app,canvas,frame);const Frame noMesh=frame;
    menu(app,0,4); // File > Import reference OBJ
    assert(app.rocket().referenceFormat==1 && app.referenceMesh().triangles.size()==1 &&
           app.selectedPart()==8 && app.page()==Page::Garage);
    render(app,canvas,frame);
    assert(noMesh!=frame && near(massProperties(app.rocket(),.0205,.0096).loaded,loaded) &&
           near(app.flight().apogee,apogee)); // reference is not physical mass
    select(app,8,1);
    assert(app.setSelectedValue(2.0)); // imported units -> millimetres
    render(app,canvas,frame);
    select(app,8,0);
    assert(!app.setSelectedValue(2) && app.rocket().referenceFormat==1 &&
           app.referenceMesh().triangles.size()==1); // missing STL leaves OBJ intact
    app.action(Action::ExportObj);
    assert(app.status().find("exported")!=std::string::npos);
    ReferenceMesh exported;
    std::string error;
    assert(exported.load("out/reference-ui/assembly.obj",error) &&
           exported.triangles.size()>800);
    app.action(Action::Save);
    Rocket persisted;
    assert(loadProject(persisted,file) && persisted.referenceFormat==1 &&
           near(persisted.referenceUnitMm,2));
    app.action(Action::NewProject);
    assert(app.referenceMesh().triangles.empty());
    app.action(Action::Load);
    assert(app.rocket().referenceFormat==1 && app.referenceMesh().triangles.size()==1);
    f=std::fopen("out/reference-ui/reference.stl","wb");
    assert(f && std::fputs("solid test\nfacet normal 0 0 1\nouter loop\n"
        "vertex 0 0 0\nvertex 0 10 0\nvertex 10 0 0\n"
        "endloop\nendfacet\nendsolid test\n",f)>0 && std::fclose(f)==0);
    menu(app,0,5); // File > Import reference STL
    assert(app.rocket().referenceFormat==2 && app.referenceMesh().triangles.size()==1);
    app.action(Action::Undo);
    assert(app.rocket().referenceFormat==1 && app.referenceMesh().triangles.size()==1);
    std::remove(file);std::remove(mesh);
    std::remove("out/reference-ui/reference.stl");
    std::remove("out/reference-ui/assembly.obj");
    std::puts("PASS reference: OBJ/STL file actions, wireframe, scale, .eul link, undo, visual export");
}
void testStation() {
    const char* path="out/recording.csv";
    std::remove(path);
    App app;
    assert(app.init("assets/font-atlas.bin","assets/motors","out/station.eul"));
    auto frame=pixels();Canvas canvas(frame.data()+1,w,h,app.font());
    app.action(Action::Ground);
    render(app,canvas,frame);
    const Frame empty=frame;
    app.action(Action::TogglePlay);
    assert(app.status().find("Load")!=std::string::npos);
    app.action(Action::LoadRecording);
    assert(app.recording().samples.empty() &&
           app.status().find("Cannot open")!=std::string::npos);
    FILE* file=std::fopen(path,"wb");
    assert(file && std::fputs("time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s\n",file)>0);
    for(int i=0;i<=40;++i) {
        const double altitude=100+(i>20?(i-20)*.2:0);
        const double yaw=i>20?22:0;
        assert(std::fprintf(file,"%.2f,%.3f,0.10,%.1f,0\n",i*.1,altitude,yaw)>0);
    }
    assert(std::fclose(file)==0);
    menu(app,0,3); // File > Load recorded CSV, same path as project directory
    assert(app.page()==Page::Ground && app.recording().samples.size()==41 &&
           !app.recording().calibrated);
    render(app,canvas,frame);
    assert(empty!=frame);
    const Frame raw=frame;
    app.click(1005,649); // real inspector calibration button
    assert(app.recording().calibrated &&
           app.status().find("altitude zero")!=std::string::npos);
    render(app,canvas,frame);
    assert(raw!=frame);
    const Frame calibrated=frame;
    app.click(806,631); // scrub recorded altitude; do not switch to Flight
    assert(app.page()==Page::Ground &&
           app.status().find("cursor")!=std::string::npos);
    render(app,canvas,frame);
    assert(calibrated!=frame);
    app.key("F4"); // view preset changes the actual gyro-driven mesh
    const Frame beforeTop=frame;
    render(app,canvas,frame);
    assert(beforeTop!=frame);
    app.key("F3");render(app,canvas,frame);
    app.action(Action::TogglePlay);const Frame beforePlay=frame;
    app.tick(.2);render(app,canvas,frame);
    assert(beforePlay!=frame && app.page()==Page::Ground);
    file=std::fopen(path,"wb");
    assert(file && std::fputs("malformed\n",file)>0 && std::fclose(file)==0);
    app.action(Action::LoadRecording);
    assert(app.recording().calibrated && app.recording().samples.size()==41);
    std::remove(path);
    std::puts("PASS station: recorded CSV, calibration, scrub/playback, relative pose and rejected file");
}
struct FakeBench final : BenchControl {
    bool active=false,gate=true;
    int connects=0,closes=0,pulses=0,neutrals=0,axis=-1,angle=0;
    bool connect(std::string& error) override {
        ++connects;active=true;error.clear();return true;
    }
    bool pulse(uint8_t channel,int cdeg,uint16_t duration,
               std::string& error) override {
        assert(active && gate && duration==150 &&
               bench::safePulse(channel,cdeg,duration));
        ++pulses;axis=channel;angle=cdeg;error.clear();return true;
    }
    bool neutral(std::string& error) override {
        assert(active);++neutrals;error.clear();return true;
    }
    void close() override {if(active){++closes;++neutrals;}active=false;}
    bool connected() const override {return active;}
    bool physicalGate() const override {return gate;}
};
void testBenchUI() {
    App app;
    assert(app.init("assets/font-atlas.bin","assets/motors","out/bench-ui.eul"));
    auto frame=pixels();Canvas canvas(frame.data()+1,w,h,app.font());
    render(app,canvas,frame);
    const Frame design=frame;
    app.click(892,85); // toolbar Bench, no bridge in host preview
    assert(app.page()==Page::Bench);
    render(app,canvas,frame);
    assert(frame!=design);
    app.click(1000,480);
    assert(!app.benchConfirmationOpen());
    app.click(1080,330);
    assert(app.status().find("No Orbis bench")!=std::string::npos);
    FakeBench fake;
    app.setBenchControl(&fake);
    app.click(1080,330);
    assert(fake.connects==1 && fake.pulses==0);
    app.action(Action::Undo);
    assert(app.status().find("isolated bench")!=std::string::npos && fake.pulses==0);
    app.click(1150,480); // +2 deg opens a modal, not a servo command
    assert(app.benchConfirmationOpen() && fake.pulses==0);
    const Frame beforeModal=frame;
    render(app,canvas,frame);
    assert(frame!=beforeModal);
    app.key("Enter"); // Cancel is the default
    assert(!app.benchConfirmationOpen() && fake.pulses==0);
    app.click(1150,480);
    app.key("ArrowRight");app.key("Enter"); // neutral works even over the modal
    assert(!app.benchConfirmationOpen() && fake.neutrals==1 && fake.pulses==0);
    app.click(1150,480);
    app.key("ArrowRight");app.key("ArrowRight");app.key("Enter");
    assert(!app.benchConfirmationOpen() && fake.pulses==1 &&
           fake.axis==0 && fake.angle==200);
    app.click(1000,424); // previous channel wraps 0 -> 3
    app.click(1000,480);
    assert(app.benchConfirmationOpen() && fake.pulses==1);
    app.click(745,434); // explicit send confirmation
    assert(!app.benchConfirmationOpen() && fake.pulses==2 &&
           fake.axis==3 && fake.angle==-200);
    app.key("n"); // host shortcut requests neutral, never a model change
    assert(fake.neutrals==2);
    app.action(Action::BenchPulsePlus);
    assert(app.benchConfirmationOpen());
    app.action(Action::BenchDisconnect); // must override the modal immediately
    assert(!app.benchConfirmationOpen() && fake.closes==1 && fake.pulses==2);
    app.action(Action::BenchConnect);fake.gate=false;
    app.action(Action::BenchPulsePlus);
    assert(!app.benchConfirmationOpen() && fake.pulses==2);
    app.action(Action::Garage);
    assert(fake.closes==2 && !fake.connected());
    app.action(Action::BenchPulsePlus); // never actuate from flight/design
    assert(fake.pulses==2);
    std::puts("PASS bench UI: opt-in, physical gate, modal default cancel, ACK path, emergency neutral and exit");
}
void testReplay() {
    App app;
    assert(app.init("assets/font-atlas.bin","assets/motors","out/replay-test.eul"));
    auto frame=pixels();Canvas canvas(frame.data()+1,w,h,app.font());
    for(double second:{0.,5.,12.,20.,26.,32.,36.,42.,46.,53.,60.,65.,72.,76.}) {
        app.demoAt(second);
        render(app,canvas,frame);
        if(second>=44 && second<73)assert(app.page()==Page::Flight && app.flight().valid);
    }
    assert(app.page()==Page::Ground);
    std::puts("PASS replay: every phase uses a valid calculated run, no synthetic telemetry");
}
}
int main() {
    testEveryProperty();
    testFocusNavigation();
    testCommandsAndInput();
    testReferenceUI();
    testStation();
    testBenchUI();
    testReplay();
}
