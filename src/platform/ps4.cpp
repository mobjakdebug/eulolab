// Orbis-only entry point. OpenOrbis PS4 Toolchain v0.5.4 headers / stubs.
// VideoOut + direct memory follows the documented OpenOrbis graphics sample
// API contract; no Linux, browser, SDL or network service runs on the PS4.
#include "ui/app.hpp"
#include "ps4_bench_usb.hpp"
#include <orbis/libkernel.h>
#include <orbis/VideoOut.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <orbis/Sysmodule.h>
#include <orbis/Usbd.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace {
constexpr int width=1280,height=720,buffers=2;
constexpr size_t frameBytes=static_cast<size_t>(width)*height*sizeof(uint32_t);

// Enumeration is informational; only the explicit Bench page may open the
// pinned board, check its vendor interface and perform a nonce handshake.
void scanUsb(eulo::App& app,const eulo::OrbisBenchUsb& usb) {
    if(!usb.configured()) {
        app.setUsbStatus("VID/PID not configured");return;
    }
    libusb_device** devices=nullptr;
    const int count=sceUsbdGetDeviceList(&devices);
    if(count<0) {
        if(devices)sceUsbdFreeDeviceList(devices);
        app.setUsbStatus("USB enumeration failed");return;
    }
    int known=0;
    for(int i=0;devices && i<count;++i) {
        libusb_device_descriptor desc{};
        if(sceUsbdGetDeviceDescriptor(devices[i],&desc)==0 &&
           desc.idVendor==usb.vendor() && desc.idProduct==usb.product())++known;
    }
    if(devices)sceUsbdFreeDeviceList(devices);
    char label[64];
    std::snprintf(label,sizeof(label),"Bench USB ID matches: %d",known);
    app.setUsbStatus(label);
}

struct Display {
    int handle=-1, frame=0;
    off_t offset=0;
    size_t bytes=0;
    void* memory=nullptr;
    OrbisKernelEqueue queue{};
    bool queueReady=false;
    bool allocated=false;
    bool registered=false;
    bool init() {
        handle=sceVideoOutOpen(ORBIS_VIDEO_USER_MAIN,ORBIS_VIDEO_OUT_BUS_MAIN,0,nullptr);
        if(handle<0)return false;
        if(sceKernelCreateEqueue(&queue,"eulolab flip queue")<0)return false;
        queueReady=true;
        if(sceVideoOutAddFlipEvent(queue,handle,nullptr)<0)return false;
        bytes=((frameBytes*buffers+0x1fffff)/0x200000)*0x200000;
        if(sceKernelAllocateDirectMemory(0,sceKernelGetDirectMemorySize(),bytes,0x200000,3,&offset)<0)return false;
        allocated=true;
        if(sceKernelMapDirectMemory(&memory,bytes,0x33,0,offset,0x200000)<0)return false;
        void* surface[buffers]={memory,static_cast<unsigned char*>(memory)+frameBytes};
        OrbisVideoOutBufferAttribute attr{};
        sceVideoOutSetBufferAttribute(&attr,0x80000000,1,0,width,height,width);
        if(sceVideoOutRegisterBuffers(handle,0,surface,buffers,&attr)<0)return false;
        registered=true;
        sceVideoOutSetFlipRate(handle,0);
        return true;
    }
    uint32_t* pixels() const {
        return reinterpret_cast<uint32_t*>(static_cast<unsigned char*>(memory)+frameBytes*(frame%buffers));
    }
    void flip() {
        const int index=frame%buffers;
        if(sceVideoOutSubmitFlip(handle,index,ORBIS_VIDEO_OUT_FLIP_VSYNC,frame)<0)return;
        for(;;) {
            OrbisVideoOutFlipStatus status{};
            if(sceVideoOutGetFlipStatus(handle,&status)<0 || status.flipArg==frame)break;
            OrbisKernelEvent event{};
            int received=0;
            if(sceKernelWaitEqueue(queue,&event,1,&received,nullptr)<0)break;
        }
        ++frame;
    }
    void shutdown() {
        if(registered) sceVideoOutUnregisterBuffers(handle,0);
        if(memory) sceKernelMunmap(memory,bytes);
        if(allocated) sceKernelReleaseDirectMemory(offset,bytes);
        if(queueReady) sceKernelDeleteEqueue(queue);
        if(handle>=0) sceVideoOutClose(handle);
    }
};

void control(eulo::App& app,uint32_t pressed) {
    if(pressed&ORBIS_PAD_BUTTON_CIRCLE) app.action(eulo::Action::Back);
    if(pressed&ORBIS_PAD_BUTTON_L1) app.action(eulo::Action::PrevPart);
    if(pressed&ORBIS_PAD_BUTTON_R1) app.action(eulo::Action::NextPart);
    if(pressed&ORBIS_PAD_BUTTON_L2) app.action(eulo::Action::FocusPrevious);
    if(pressed&ORBIS_PAD_BUTTON_R2) app.action(eulo::Action::FocusNext);
    if(pressed&ORBIS_PAD_BUTTON_UP) app.key("ArrowUp");
    if(pressed&ORBIS_PAD_BUTTON_DOWN) app.key("ArrowDown");
    if(pressed&ORBIS_PAD_BUTTON_LEFT) app.key("ArrowLeft");
    if(pressed&ORBIS_PAD_BUTTON_RIGHT) app.key("ArrowRight");
    if(pressed&ORBIS_PAD_BUTTON_SQUARE)
        app.action(app.page()==eulo::Page::Bench?
                   eulo::Action::BenchNeutral:eulo::Action::Save);
    if(pressed&ORBIS_PAD_BUTTON_OPTIONS) app.action(eulo::Action::Menu);
    if(pressed&ORBIS_PAD_BUTTON_R3) app.action(eulo::Action::CycleView);
    if(pressed&ORBIS_PAD_BUTTON_TRIANGLE)
        app.action(app.page()==eulo::Page::Ground?eulo::Action::Garage:eulo::Action::Ground);
    if(pressed&ORBIS_PAD_BUTTON_CROSS) app.action(eulo::Action::Confirm);
}
} // namespace

int main() {
    eulo::App app;
    if(!app.init("/app0/assets/font-atlas.bin","/app0/assets/motors",
                 "/data/eulolab/project.eul"))return 1;
    // The app owns a separately saved, user-edited hardware profile. No USB
    // identity or pin mapping is compiled into the PS4 binary.
    eulo::OrbisBenchUsb usb(app.benchProfile());
    // Nonce detects stale/cross-session replies; it is not authentication.
    eulo::BenchLink benchLink(usb,app.benchProfile(),
                              static_cast<uint32_t>(sceKernelGetProcessTime()));
    // The console has a controller but no text-entry backend in this build.
    // Numeric values step on X or the D-pad rather than opening a dead textbox.
    app.setTextEntryAvailable(false);
    app.setRuntimeLabel("ORBIS BUILD / CONSOLE UNVERIFIED");
    if(mkdir("/data/eulolab",0755)<0 && errno!=EEXIST) {
        // Simulation still works even if sandbox policy forbids local storage.
    }
    Display display;
    if(!display.init()) { display.shutdown();return 2; }
    sceUserServiceInitialize(nullptr);
    int user=-1;
    sceUserServiceGetInitialUser(&user);
    const int pad=scePadInit()==0?scePadOpen(user,0,0,nullptr):-1;
    uint32_t previous=0;
    bool padWasConnected=false;
    bool usbReady=false;
    if(sceSysmoduleLoadModule(ORBIS_SYSMODULE_USBD)==0 && sceUsbdInit()==0) {
        usbReady=true;
        app.setBenchControl(&benchLink);
        scanUsb(app,usb);
    } else app.setUsbStatus("USB module unavailable");
    uint64_t last=sceKernelGetProcessTime(),lastScan=last;
    for(;;) {
        const uint64_t now=sceKernelGetProcessTime();
        const double seconds=static_cast<double>(now)/1000000.0;
        const double dt=static_cast<double>(now-last)/1000000.0;
        last=now;
        if(pad>=0) {
            OrbisPadData state{};
            if(scePadReadState(pad,&state)>=0 && state.connected) {
                control(app,state.buttons & ~previous);
                const auto stick=[](uint8_t raw) {
                    const double value=(static_cast<int>(raw)-128)/127.0;
                    return std::fabs(value)<0.16?0.0:value;
                };
                const double step=std::clamp(dt,0.0,0.05)*2.0;
                app.orbit(stick(state.rightStick.x)*step,
                          -stick(state.rightStick.y)*step);
                previous=state.buttons;
                padWasConnected=true;
            } else {
                previous=0;
                if(padWasConnected && app.page()==eulo::Page::Bench)
                    app.action(eulo::Action::BenchDisconnect);
                padWasConnected=false;
            }
        }
        if(usbReady && (app.page()==eulo::Page::Ground ||
                        app.page()==eulo::Page::Bench) && now-lastScan>2000000) {
            // Informational discovery reflects edits made inside Bench setup;
            // it never identifies a board or opens a connection automatically.
            scanUsb(app,usb);lastScan=now;
        }
        app.tick(dt);
        eulo::Canvas canvas(display.pixels(),width,height,app.font());
        app.draw(canvas,seconds);
        display.flip();
    }
    // The app is exited by the PS4 shell. On explicit termination these would
    // close the pad and USB module and release registered display buffers.
    if(pad>=0) scePadClose(pad);
    if(usbReady) sceUsbdExit();
    display.shutdown();
    return 0;
}
