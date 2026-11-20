// Host renderer of the exact C++ UI and physics core used in the PS4 build.
// It has no browser runtime and never claims to emulate the Orbis hardware.
#include "ui/app.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

int main(int argc, char** argv) {
    eulo::App app;
    if(!app.init("assets/font-atlas.bin","assets/motors","out/project.ini")) {
        std::fprintf(stderr,"Cannot load font atlas or commercial .eng curves\n");return 1;
    }
    constexpr int width=1280,height=720;
    std::vector<uint32_t> pixels(static_cast<size_t>(width)*height);
    eulo::Canvas canvas(pixels.data(),width,height,app.font());
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
    std::fprintf(stderr,"Usage: %s --stats | --screenshot OUTPUT.ppm [demo_second] | --demo-raw [fps] [seconds]\n",argv[0]);
    return 2;
}
