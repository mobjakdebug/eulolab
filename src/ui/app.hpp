#pragma once
#include "canvas.hpp"
#include "core/bench_link.hpp"
#include "core/flight.hpp"
#include "core/telemetry.hpp"
#include "rocket_view.hpp"
#include <string>

namespace eulo {
enum class Page { Garage, Flight, Ground, Bench };
enum class Action {
    NewProject, Garage, Flight, Ground, Back, NextPart, PrevPart,
    NextField, PrevField, Increase, Decrease, TogglePlay, Save, Load,
    Menu, Confirm, Undo, ResetPart, JumpApogee, FitView,
    HelpControls, HelpModel, FocusNext, FocusPrevious,
    SideView, TopView, IsoView, CycleView, LoadRecording, CalibrateRecording,
    ImportObj, ImportStl, ExportObj,
    Bench, BenchConnect, BenchDisconnect, BenchAxisPrev, BenchAxisNext,
    BenchPulseMinus, BenchPulsePlus, BenchNeutral
};

class App {
public:
    bool init(const char* fontPath,const char* motorDirectory,const char* projectPath);
    void draw(Canvas& c,double now) const;
    void action(Action a);
    void key(const std::string& name,bool ctrl=false,bool shift=false);
    void typeCharacter(char ch);
    void click(int x,int y);
    void hover(int x,int y);
    void wheel(int x,int y,int direction);
    void tick(double seconds);
    void orbit(double yawRadians,double pitchRadians);
    void demoAt(double seconds);
    const FontAtlas& font() const { return font_; }
    const Rocket& rocket() const { return rocket_; }
    const FlightResult& flight() const { return flight_; }
    const TelemetryRecording& recording() const { return recording_; }
    const ReferenceMesh& referenceMesh() const { return reference_; }
    bool importTelemetry(const char* path);
    bool importReference(int format);
    Page page() const { return page_; }
    int selectedPart() const { return selectedPart_; }
    int selectedField() const { return selectedField_; }
    int fieldCount(int part) const;
    bool setSelectedValue(double value);
    bool menuOpen() const { return menuOpen_; }
    bool discardConfirmationOpen() const { return confirmDiscard_; }
    bool editing() const { return editing_; }
    const std::string& status() const { return status_; }
    void setUsbStatus(const std::string& text) { usbStatus_ = text; }
    void setBenchControl(BenchControl* bench);
    bool benchConfirmationOpen() const { return benchConfirm_; }
    void setRuntimeLabel(const std::string& text) { runtimeLabel_ = text; }
    void setTextEntryAvailable(bool enabled) { textEntryAvailable_ = enabled; }
private:
    struct Field {
        const char* name;
        const char* unit;
        double value,min,max,step;
        int decimals;
        bool choice;
    };
    FontAtlas font_;
    Rocket rocket_;
    Rocket previousRocket_;
    MotorCurve motors_[2];
    const MotorCurve& motor() const { return motors_[rocket_.motorIndex]; }
    FlightResult flight_;
    TelemetryRecording recording_;
    ReferenceMesh reference_;
    Page page_ = Page::Garage;
    int selectedPart_ = 0, selectedField_ = 0;
    int hoverX_ = -1, hoverY_ = -1;
    int focusRegion_ = 3, toolbarFocus_ = 0;
    int menuIndex_ = 0, menuItem_ = 0, helpMode_ = 0;
    int benchAxis_ = 0, benchFocus_ = 0, benchAngleCdeg_ = 0;
    int benchConfirmFocus_ = 0;
    bool benchConfirm_ = false;
    BenchControl* bench_ = nullptr;
    bool menuOpen_ = false, hasUndo_ = false, dirty_ = false, saved_ = false;
    bool confirmDiscard_ = false, skipDiscardOnce_ = false;
    Action pendingAction_ = Action::NewProject;
    int discardFocus_ = 0; // cancel by default
    bool editing_ = false, editReplace_ = false, textEntryAvailable_ = true;
    std::string editBuffer_;
    double simTime_ = 0, groundTime_ = 0, yawOffset_ = 0, pitchOffset_ = 0, zoom_ = 1;
    int cameraPreset_ = 0;
    bool playing_ = false, demoMode_ = false;
    int demoStage_ = -1;
    std::string projectPath_;
    std::string recordingPath_;
    std::string status_ = "Select a component and edit its dimensions.";
    std::string usbStatus_ = "USB not yet scanned";
    std::string runtimeLabel_ = "NATIVE WORKBENCH";

    void rebuild();
    std::string projectDirectory() const;
    bool loadReference(int format,ReferenceMesh& mesh,std::string& error) const;
    Field field(int part,int index) const;
    std::string valueText(int part,int index) const;
    void editField(int direction);
    bool applyField(int part,int index,double value);
    void resetSelectedPart();
    void beginEdit();
    void commitEdit();
    void cancelEdit();
    void runMenuItem();
    int menuCount(int menu) const;
    void drawChrome(Canvas& c) const;
    void drawTree(Canvas& c) const;
    void drawInspector(Canvas& c) const;
    void drawAnalysis(Canvas& c) const;
    void drawStatus(Canvas& c) const;
    void drawMenu(Canvas& c) const;
    void drawHelp(Canvas& c) const;
    void drawDiscardConfirmation(Canvas& c) const;
    void drawBenchConfirmation(Canvas& c) const;
    void leaveBench();
};
} // namespace eulo
