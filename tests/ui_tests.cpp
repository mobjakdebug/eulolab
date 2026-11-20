#include "ui/app.hpp"
#include "core/rocket.hpp"
#include <cassert>
#include <cstdio>
#include <vector>

int main() {
    using namespace eulo;
    App app;
    assert(app.init("assets/font-atlas.bin","assets/motors","out/ui-project.ini"));
    constexpr int w=1280,h=720;
    constexpr uint32_t guard=0x12345678;
    std::vector<uint32_t> bytes(static_cast<size_t>(w)*h+2,guard);
    Canvas canvas(bytes.data()+1,w,h,app.font());
    for(double seconds : {0.,5.,12.,20.,26.,32.,36.,42.,46.,53.,60.,72.,76.}) {
        app.demoAt(seconds);
        app.draw(canvas,seconds);
        assert(bytes.front()==guard && bytes.back()==guard);
        assert(bytes[static_cast<size_t>(300)*w+600]!=guard); // actual pixels drawn
    }
    assert(app.page()==Page::Ground);
    app.action(Action::NewProject);
    assert(app.page()==Page::Garage);
    // Change material through the same input actions as the PS4 pad.
    for(int i=0;i<3;++i)app.action(Action::NextField);
    app.action(Action::Increase);
    app.action(Action::NextPart);
    app.action(Action::Increase); // nose length
    for(int i=0;i<3;++i)app.action(Action::NextPart);
    app.action(Action::Increase); // choose D9
    app.action(Action::Save);
    Rocket saved;
    assert(loadProject(saved,"out/ui-project.ini"));
    assert(saved.material==Material::Aluminium && saved.motorIndex==1 &&
           saved.noseLength > Rocket{}.noseLength);
    app.action(Action::NewProject);
    app.action(Action::Load);
    app.action(Action::Save);
    Rocket restored;
    assert(loadProject(restored,"out/ui-project.ini"));
    assert(restored.material==saved.material && restored.motorIndex==saved.motorIndex);
    app.action(Action::Flight);
    assert(app.page()==Page::Flight);
    app.tick(0.1);
    app.action(Action::TogglePlay);
    app.action(Action::Increase);
    app.draw(canvas,7);
    assert(bytes.front()==guard && bytes.back()==guard);
    app.action(Action::Ground);
    assert(app.page()==Page::Ground);
    app.action(Action::Back);
    assert(app.page()==Page::Garage);
    app.draw(canvas,10);
    const auto beforeOrbit=bytes;
    app.orbit(0.8,0.2);
    app.draw(canvas,10);
    assert(beforeOrbit!=bytes && bytes.front()==guard && bytes.back()==guard);
    std::remove("out/ui-project.ini");
    std::puts("PASS UI: scripted host rendering, guard pixels, pad actions, save/load, and navigation");
}
