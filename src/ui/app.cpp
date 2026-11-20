#include "app.hpp"
#include "workbench_layout.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace eulo {
namespace {
constexpr double pi = 3.14159265358979323846;
constexpr int counts[] = {6,5,9,2,6,4};
std::string number(double value,int decimals) {
    char text[64];
    std::snprintf(text,sizeof(text),"%.*f",decimals,value);
    return text;
}
}

bool App::init(const char* fontPath,const char* motorDirectory,const char* projectPath) {
    if (!font_.load(fontPath) || !motorDirectory) return false;
    projectPath_ = projectPath ? projectPath : "project.ini";
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
int App::fieldCount(int part) const { return part>=0 && part<6 ? counts[part] : 0; }

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
    }
    if(part==5 && index==0 && rocket_.parachuteDiameter==0)return "OFF";
    return number(f.value,f.decimals);
}

bool App::applyField(int part,int index,double value) {
    if(part<0 || part>=6 || index<0 || index>=fieldCount(part) || !std::isfinite(value))return false;
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
    }
    const bool geometryValid=rocket_.valid();
    const bool motorFit=geometryValid && motorFits(rocket_,motor());
    if(!geometryValid || !motorFit) {
        rocket_=before;
        status_=geometryValid?
            "Rejected: motor casing must fit fully inside the mount.":
            "Rejected: dependent geometry must fit inside the body.";
        return false;
    }
    previousRocket_=before;
    hasUndo_=true;
    dirty_=true;
    rebuild();
    status_=std::string(f.name)+" changed; mass and flight model recalculated.";
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
    }
    if(!rocket_.valid() || !motorFits(rocket_,motor())) {
        rocket_=before;
        status_="Reset rejected: another component depends on this geometry.";
        return;
    }
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
    switch(a) {
        case Action::NewProject:
            rocket_=Rocket{};rebuild();page_=Page::Garage;selectedPart_=0;
            selectedField_=0;simTime_=0;playing_=false;dirty_=false;saved_=false;hasUndo_=false;
            status_="New project. Select any component to edit.";break;
        case Action::Garage:page_=Page::Garage;focusRegion_=3;playing_=false;
            status_="Assembly workspace.";break;
        case Action::Flight:
            if(flight_.valid && !flight_.samples.empty()) {
                page_=Page::Flight;focusRegion_=4;simTime_=0;playing_=true;
                status_="Playing the educational 1D flight model.";
            } else status_="Simulation unavailable: inspect geometry and motor fit.";
            break;
        case Action::Ground:
            page_=Page::Ground;focusRegion_=5;playing_=false;
            status_="USB discovery only; no telemetry is being read.";break;
        case Action::Back:
            if(menuOpen_)menuOpen_=false;
            else if(helpMode_)helpMode_=0;
            else if(page_!=Page::Garage) {page_=Page::Garage;focusRegion_=3;playing_=false;}
            break;
        case Action::NextPart:selectedPart_=(selectedPart_+1)%6;selectedField_=0;
            focusRegion_=2;page_=Page::Garage;break;
        case Action::PrevPart:selectedPart_=(selectedPart_+5)%6;selectedField_=0;
            focusRegion_=2;page_=Page::Garage;break;
        case Action::NextField:selectedField_=(selectedField_+1)%fieldCount(selectedPart_);
            focusRegion_=3;break;
        case Action::PrevField:selectedField_=(selectedField_+fieldCount(selectedPart_)-1)%fieldCount(selectedPart_);
            focusRegion_=3;break;
        case Action::Increase:
            if(page_==Page::Garage)editField(1);
            else if(page_==Page::Flight && !flight_.samples.empty())
                simTime_=std::min(flight_.samples.back().time,simTime_+.5);
            break;
        case Action::Decrease:
            if(page_==Page::Garage)editField(-1);
            else if(page_==Page::Flight)simTime_=std::max(0.0,simTime_-.5);
            break;
        case Action::TogglePlay:
            if(page_!=Page::Flight) {action(Action::Flight);break;}
            if(!playing_ && flight_.valid && !flight_.samples.empty() &&
               simTime_>=flight_.samples.back().time-1e-6)simTime_=0;
            playing_=!playing_;status_=playing_?"Playing flight model.":"Playback paused.";break;
        case Action::Save:
            if(saveProject(rocket_,projectPath_.c_str())) {
                dirty_=false;saved_=true;status_="Project saved.";
            } else status_="Save failed: verify the project storage directory.";
            break;
        case Action::Load:
            if(loadProject(rocket_,projectPath_.c_str())) {
                rebuild();dirty_=false;saved_=true;page_=Page::Garage;playing_=false;
                status_=flight_.valid?"Project loaded.":"Project loaded; motor clearance invalid.";
            } else status_="Open failed: no valid saved project at this location.";
            break;
        case Action::Menu:menuOpen_=true;menuItem_=0;focusRegion_=0;break;
        case Action::Confirm:
            if(focusRegion_==3) {
                if(page_==Page::Garage)beginEdit();
                else action(Action::Garage);
            }
            else if(focusRegion_==1)action(workbench::tools[toolbarFocus_].action);
            else if(focusRegion_==0)action(Action::Menu);
            else if(focusRegion_==4)action(Action::TogglePlay);
            else if(focusRegion_==5)action(page_==Page::Garage?
                Action::ResetPart:Action::Garage);
            break;
        case Action::Undo:
            if(hasUndo_) {
                rocket_=previousRocket_;hasUndo_=false;dirty_=true;rebuild();
                status_="Last model change undone.";
            } else status_="Nothing to undo.";
            break;
        case Action::ResetPart:resetSelectedPart();break;
        case Action::JumpApogee:
            if(flight_.valid && !flight_.samples.empty()) {
                page_=Page::Flight;focusRegion_=4;playing_=false;simTime_=flight_.apogeeTime;
                status_="At model apogee. Drag the time cursor to inspect the run.";
            } else status_="No valid flight to inspect.";
            break;
        case Action::FitView:zoom_=1;yawOffset_=0;pitchOffset_=0;
            status_="Viewport fitted to the model.";break;
        case Action::HelpControls:helpMode_=1;status_="Controls reference.";break;
        case Action::HelpModel:helpMode_=2;status_="Model assumptions and limitations.";break;
        case Action::FocusNext:focusRegion_=(focusRegion_+1)%6;break;
        case Action::FocusPrevious:focusRegion_=(focusRegion_+5)%6;break;
    }
}
void App::tick(double seconds) {
    if(!playing_ || page_!=Page::Flight || !flight_.valid || flight_.samples.empty())return;
    simTime_+=std::clamp(seconds,0.0,0.25);
    if(simTime_>=flight_.samples.back().time) {
        simTime_=flight_.samples.back().time;playing_=false;
        status_="Playback complete; model landed or ended.";
    }
}
void App::orbit(double yawRadians,double pitchRadians) {
    if(!std::isfinite(yawRadians) || !std::isfinite(pitchRadians))return;
    yawOffset_=std::remainder(yawOffset_+std::clamp(yawRadians,-0.2,0.2),2*pi);
    pitchOffset_=std::clamp(pitchOffset_+std::clamp(pitchRadians,-0.2,0.2),-0.4,0.4);
}

void App::key(const std::string& name,bool ctrl,bool shift) {
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
    if(name=="ArrowUp") {
        if(focusRegion_==2)action(Action::PrevPart);
        else if(focusRegion_==1)toolbarFocus_=(toolbarFocus_+9)%10;
        else action(Action::PrevField);
    } else if(name=="ArrowDown") {
        if(focusRegion_==2)action(Action::NextPart);
        else if(focusRegion_==1)toolbarFocus_=(toolbarFocus_+1)%10;
        else action(Action::NextField);
    } else if(name=="ArrowLeft") {
        if(focusRegion_==1)toolbarFocus_=(toolbarFocus_+9)%10;
        else if(focusRegion_==2)action(Action::PrevPart);
        else action(Action::Decrease);
    } else if(name=="ArrowRight") {
        if(focusRegion_==1)toolbarFocus_=(toolbarFocus_+1)%10;
        else if(focusRegion_==2)action(Action::NextPart);
        else action(Action::Increase);
    } else if(name=="Enter" || name==" ")action(Action::Confirm);
    else if(name.size()==1 && name[0]>='1' && name[0]<='6') {
        selectedPart_=name[0]-'1';selectedField_=0;page_=Page::Garage;focusRegion_=2;
    }
}

void App::click(int x,int y) {
    if(x<0 || x>=1280 || y<0 || y>=720)return;
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
        for(int i=0;i<10;++i) {
            const auto& tool=workbench::tools[i];
            if(x>=tool.x && x<tool.x+tool.w) {
                toolbarFocus_=i;focusRegion_=1;action(tool.action);return;
            }
        }
    }
    if(x<workbench::treeW && y>=workbench::treeRowY &&
       y<workbench::treeRowY+6*workbench::treeRowH) {
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
        action(page_==Page::Garage?Action::ResetPart:Action::Garage);
        return;
    }
    if(page_!=Page::Ground && x>=314 && x<=919 && y>=604 && y<=660 &&
       flight_.valid && !flight_.samples.empty()) {
        simTime_=std::clamp((x-314)/605.0,0.0,1.0)*flight_.samples.back().time;
        page_=Page::Flight;
        playing_=false;focusRegion_=4;status_="Flight cursor set from timeline.";
    }
}
void App::hover(int x,int y) {
    hoverX_=x>=0 && x<1280?x:-1;
    hoverY_=y>=0 && y<720?y:-1;
}
void App::wheel(int x,int y,int direction) {
    if(editing_) {commitEdit();if(editing_)return;}
    if(menuOpen_ || helpMode_ || confirmDiscard_)return;
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
    const int stage=seconds<6?0:seconds<12?1:seconds<18?2:seconds<26?3:
        seconds<34?4:seconds<44?5:seconds<73?6:7;
    if(stage!=demoStage_) {
        demoStage_=stage;
        rocket_=Rocket{};
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
            "USB discovery only; there is no live telemetry.";
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
