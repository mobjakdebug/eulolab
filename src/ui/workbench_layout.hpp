#pragma once
#include "app.hpp"

namespace eulo::workbench {
constexpr int titleH=32, menuY=32, menuH=30, toolbarY=62, toolbarH=46;
constexpr int contentY=108, statusY=681;
constexpr int treeW=244, inspectorX=960;
constexpr int viewX=244, viewY=108, viewW=716, viewH=450;
constexpr int analysisY=558, analysisH=123;
constexpr int fieldY=178, fieldH=42, inputX=976, inputW=288;
constexpr int treeRowY=170, treeRowH=52;

struct MenuItem { const char* label; Action action; };
struct Menu { const char* label; int x,w,count; MenuItem items[4]; };
inline constexpr Menu menus[] = {
    {"File",12,51,3,{{"New project",Action::NewProject},{"Open project",Action::Load},
                      {"Save project",Action::Save},{"",Action::Save}}},
    {"Edit",64,52,2,{{"Undo last edit",Action::Undo},{"Reset component",Action::ResetPart},
                       {"",Action::Save},{"",Action::Save}}},
    {"View",117,56,4,{{"Assembly workspace",Action::Garage},{"Flight analysis",Action::Flight},
                       {"USB diagnostics",Action::Ground},{"Fit viewport",Action::FitView}}},
    {"Simulation",174,118,3,{{"Run from start",Action::Flight},{"Play / pause",Action::TogglePlay},
                               {"Jump to apogee",Action::JumpApogee},{"",Action::Save}}},
    {"Help",293,58,2,{{"Controls",Action::HelpControls},{"Model limitations",Action::HelpModel},
                       {"",Action::Save},{"",Action::Save}}}
};
struct Tool {int x,w; const char* label; Action action;};
inline constexpr Tool tools[] = {
    {12,52,"New",Action::NewProject}, {70,62,"Open",Action::Load},
    {138,61,"Save",Action::Save}, {207,60,"Undo",Action::Undo},
    {285,86,"Design",Action::Garage}, {377,75,"Flight",Action::Flight},
    {458,56,"USB",Action::Ground}, {530,91,"Fit view",Action::FitView},
    {627,126,"Play / Pause",Action::TogglePlay}, {759,93,"Apogee",Action::JumpApogee}
};
inline constexpr const char* partNames[] = {
    "Body tube","Nose cone","Fin set","Avionics","Motor mount","Recovery"
};
inline constexpr const char* partNotes[] = {
    "6 properties","5 properties","9 properties","2 properties","6 properties","4 properties"
};
}
