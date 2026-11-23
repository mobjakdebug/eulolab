// Host renderer of the exact C++ UI and physics core used in the PS4 build.
// It has no browser runtime and never claims to emulate the Orbis hardware.
#include "ui/app.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

int main(int argc, char** argv) {
    if(argc==3 && std::strcmp(argv[1],"--check-mesh")==0) {
        eulo::ReferenceMesh mesh;
        std::string error;
        return mesh.load(argv[2],error)?0:4;
    }
    if(argc==3 && std::strcmp(argv[1],"--check-recording")==0) {
        eulo::TelemetryRecording recording;
        std::string error;
        return recording.load(argv[2],error)?0:4;
    }
    if(argc==3 && std::strcmp(argv[1],"--check-fins")==0) {
        eulo::FinInspection inspection;
        std::string error;
        return inspection.load(argv[2],error)?0:4;
    }
    eulo::App app;
    if(!app.init("assets/font-atlas.bin","assets/motors","out/project.eul")) {
        std::fprintf(stderr,"Cannot load font atlas or commercial .eng curves\n");return 1;
    }
    app.setRuntimeLabel("HOST PREVIEW / NOT PS4");
    app.setUsbStatus("No host USB bridge");
    constexpr int width=1280,height=720;
    std::vector<uint32_t> pixels(static_cast<size_t>(width)*height);
    eulo::Canvas canvas(pixels.data(),width,height,app.font());
    // A minimal command pipe for the local preview server: input events go to
    // the actual shared C++ App, which returns its native framebuffer. No UI
    // or simulation logic is reimplemented in JavaScript or in the server.
    if(argc>1 && std::strcmp(argv[1],"--interactive")==0) {
        char line[256];
        while(std::fgets(line,sizeof(line),stdin)) {
            int x=0,y=0,delta=0,ctrl=0,shift=0;
            char name[48]={};double elapsed=0;
            if(std::strcmp(line,"FRAME\n")==0) {
                app.draw(canvas,0);
                if(std::fwrite(pixels.data(),sizeof(uint32_t),pixels.size(),stdout)!=pixels.size() ||
                   std::fflush(stdout)!=0)return 3;
            } else if(std::strcmp(line,"PAGE\n")==0) {
                const char page=static_cast<char>('0'+static_cast<int>(app.page()));
                if(std::fwrite(&page,1,1,stdout)!=1 || std::fflush(stdout)!=0)return 3;
            } else if(std::sscanf(line,"CLICK %d %d",&x,&y)==2)app.click(x,y);
            else if(std::sscanf(line,"HOVER %d %d",&x,&y)==2)app.hover(x,y);
            else if(std::sscanf(line,"DRAG %d %d",&x,&y)==2)
                app.orbit(x*0.004,y*0.004);
            else if(std::sscanf(line,"WHEEL %d %d %d",&x,&y,&delta)==3)
                app.wheel(x,y,delta);
            else if(std::sscanf(line,"TICK %lf",&elapsed)==1 && std::isfinite(elapsed))
                app.tick(elapsed);
            else if(std::sscanf(line,"KEY %47s %d %d",name,&ctrl,&shift)==3)
                app.key(name,ctrl!=0,shift!=0);
        }
        return 0;
    }
    if(argc>1 && std::strcmp(argv[1],"--demo-raw")==0) {
        const int fps=argc>2?std::atoi(argv[2]):15;
        const int seconds=argc>3?std::atoi(argv[3]):66;
        if(fps<1 || fps>60 || seconds<1 || seconds>240) return 2;
        for(int i=0;i<fps*seconds;++i) {
            const double time=static_cast<double>(i)/fps;
            app.demoAt(time);
            app.draw(canvas,time);
            if(std::fwrite(pixels.data(),sizeof(uint32_t),pixels.size(),stdout)!=pixels.size())return 3;
        }
        return 0;
    }
    // Sparse capture for editorial use. A line is a demo timestamp or the
    // literal BENCH/BENCH_SETUP. The latter render the *current* native UI
    // with no BenchControl attached; no static screenshots or USB simulation.
    // Each response is exactly one native BGRA framebuffer.
    if(argc==2 && std::strcmp(argv[1],"--demo-times")==0) {
        char line[80];
        while(std::fgets(line,sizeof(line),stdin)) {
            double time=0;
            if(std::strcmp(line,"BENCH\n")==0 ||
               std::strcmp(line,"BENCH_SETUP\n")==0) {
                const bool setup=std::strcmp(line,"BENCH_SETUP\n")==0;
                app.demoAt(0);
                app.action(eulo::Action::Bench);
                if(setup)app.action(eulo::Action::BenchSetup);
            } else {
                char* end=nullptr;
                time=std::strtod(line,&end);
                if(end==line || (*end!='\n' && *end!='\0') ||
                   !std::isfinite(time) || time<0 || time>240) return 2;
                app.demoAt(time);
            }
            app.draw(canvas,time);
            if(std::fwrite(pixels.data(),sizeof(uint32_t),pixels.size(),stdout)!=pixels.size() ||
               std::fflush(stdout)!=0)return 3;
        }
        return std::ferror(stdin)?3:0;
    }
    if(argc>2 && std::strcmp(argv[1],"--screenshot")==0) {
        const double time=argc>3?std::atof(argv[3]):9.0;
        if(time<0 || time>240)return 2;
        app.demoAt(time);
        app.draw(canvas,time);
        FILE* f=std::fopen(argv[2],"wb");
        if(!f)return 1;
        std::fprintf(f,"P6\n%d %d\n255\n",width,height);
        for(uint32_t pixel:pixels) {
            const unsigned char rgb[3]={static_cast<unsigned char>(pixel>>16),
                static_cast<unsigned char>(pixel>>8),static_cast<unsigned char>(pixel)};
            std::fwrite(rgb,1,3,f);
        }
        return std::fclose(f)==0?0:1;
    }
    if(argc>1 && std::strcmp(argv[1],"--stats")==0) {
        std::printf("EuloLab host core / %d x %d pixels / native PS4 app in src/platform/ps4.cpp\n",
            width,height);
        return 0;
    }
    std::fprintf(stderr,"Usage: %s --interactive | --stats | --screenshot OUTPUT.ppm [demo_second] | --check-recording/--check-fins/--check-mesh FILE | --demo-raw [fps] [seconds] | --demo-times\n",argv[0]);
    return 2;
}
