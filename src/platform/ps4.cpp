// Orbis-only entry point. OpenOrbis PS4 Toolchain v0.5.4 headers / stubs.
// VideoOut + direct memory follows the documented OpenOrbis graphics sample
// API contract; no Linux, browser, SDL or network service runs on the PS4.
#include "ui/app.hpp"
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

// Only discovery is enabled. USB-to-serial transfer is NOT implemented yet;
// never represent an enumerated device as an active telemetry connection.
int scanUsb(eulo::App& app) {
    libusb_device** devices=nullptr;
    const int count=sceUsbdGetDeviceList(&devices);
    if(count<0) { app.setUsbStatus("USB discovery error"); return count; }
    int known=0;
    for(int i=0;i<count;++i) {
        struct libusb_device_descriptor desc{};
        if(devices && sceUsbdGetDeviceDescriptor(devices[i],&desc)==0 &&
            (desc.bDeviceClass==2 || desc.idVendor==0x10c4 || desc.idVendor==0x1a86)) ++known;
    }
    if(devices) sceUsbdFreeDeviceList(devices);
    char label[72];
    std::snprintf(label,sizeof(label),known?"USB serial candidates: %d":"No USB serial device",known);
    app.setUsbStatus(label);
    return count;
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
    if(pressed&ORBIS_PAD_BUTTON_UP) app.action(eulo::Action::PrevField);
    if(pressed&ORBIS_PAD_BUTTON_DOWN) app.action(eulo::Action::NextField);
    if(pressed&ORBIS_PAD_BUTTON_LEFT) app.action(eulo::Action::Decrease);
    if(pressed&ORBIS_PAD_BUTTON_RIGHT) app.action(eulo::Action::Increase);
    if(pressed&ORBIS_PAD_BUTTON_SQUARE) app.action(eulo::Action::Save);
    if(pressed&ORBIS_PAD_BUTTON_OPTIONS) app.action(eulo::Action::Load);
    if(pressed&ORBIS_PAD_BUTTON_TRIANGLE)
        app.action(app.page()==eulo::Page::Ground?eulo::Action::Garage:eulo::Action::Ground);
    if(pressed&ORBIS_PAD_BUTTON_CROSS) {
        switch(app.page()) {
            case eulo::Page::Home: app.action(eulo::Action::NewProject);break;
            case eulo::Page::Garage: app.action(eulo::Action::Flight);break;
            case eulo::Page::Flight: app.action(eulo::Action::TogglePlay);break;
            case eulo::Page::Ground: app.action(eulo::Action::Garage);break;
        }
    }
}
} // namespace

int main() {
    eulo::App app;
    if(!app.init("/app0/assets/font-atlas.bin","/app0/assets/motors",
                 "/data/eulolab/project.ini"))return 1;
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
    bool usbReady=false;
    if(sceSysmoduleLoadModule(ORBIS_SYSMODULE_USBD)==0 && sceUsbdInit()==0) {
        usbReady=true;
        scanUsb(app);
    } else app.setUsbStatus("USB unavailable on this PS4");
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
            } else previous=0;
        }
        if(usbReady && app.page()==eulo::Page::Ground && now-lastScan>5000000) {
            scanUsb(app);lastScan=now;
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
