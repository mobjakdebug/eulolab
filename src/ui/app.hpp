#pragma once
#include "canvas.hpp"
#include "core/flight.hpp"
#include "rocket_view.hpp"
#include <string>

namespace eulo {
enum class Page { Home, Garage, Flight, Ground };
enum class Action {
    NewProject, Garage, Flight, Ground, Back, NextPart, PrevPart,
    NextField, PrevField, Increase, Decrease, TogglePlay, Save, Load
};

class App {
public:
    bool init(const char* fontPath, const char* motorDirectory, const char* projectPath);
    void draw(Canvas& c, double now) const;
    void action(Action a);
    void tick(double seconds);
    void orbit(double yawRadians, double pitchRadians); // right-stick 3D orbit
    void demoAt(double seconds); // drives the real app for reproducible footage
    const FontAtlas& font() const { return font_; }
    Page page() const { return page_; }
    void setUsbStatus(const std::string& text) { usbStatus_ = text; }
private:
    FontAtlas font_;
    Rocket rocket_;
    MotorCurve motors_[2];
    const MotorCurve& motor() const { return motors_[rocket_.motorIndex]; }
    FlightResult flight_;
    Page page_ = Page::Home;
    int selectedPart_ = 0;
    int selectedField_ = 0;
    double simTime_ = 0;
    double yawOffset_ = 0.0;
    double pitchOffset_ = 0.0;
    bool playing_ = false;
    std::string projectPath_;
    std::string status_ = "Create a project to begin.";
    std::string usbStatus_ = "Host / no USB link";
    bool demoMode_ = false;
    int demoStage_ = -1;
    void rebuild();
    int fieldCount() const;
    void editField(int dir);
    void drawChrome(Canvas& c) const;
    void drawSidebar(Canvas& c) const;
    void drawInspector(Canvas& c, double now) const;
    void drawFooter(Canvas& c) const;
    void drawHome(Canvas& c) const;
};
}
