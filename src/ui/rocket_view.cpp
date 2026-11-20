#include "rocket_view.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace eulo {
namespace {
struct Point3 { double x,y,z; };
struct Screen { int x,y; double depth; };
struct Face { Screen a,b,c; uint32_t color; double depth; };
constexpr double pi = 3.14159265358979323846;
uint32_t blend(uint32_t a,uint32_t b,double t) {
    t=std::clamp(t,0.0,1.0);
    const auto channel=[t](int p,int q){return static_cast<int>(p*(1-t)+q*t);};
    return rgb(channel((a>>16)&255,(b>>16)&255),
        channel((a>>8)&255,(b>>8)&255),channel(a&255,b&255));
}
uint32_t materialTone(Material m) {
    return m==Material::PVC?rgb(210,219,220):
           m==Material::Aluminium?rgb(181,194,203):rgb(103,118,129);
}
}

void drawRocketView(Canvas& c,const Rocket& r,const MassProperties& mass,const RocketView& v) {
    const uint32_t backdrop=rgb(234,239,241), rule=rgb(206,216,220);
    const uint32_t muted=rgb(77,93,103), axis=rgb(121,143,151), accent=rgb(35,91,147);
    c.setClip(v.x,v.y,v.w,v.h);
    c.rect(v.x,v.y,v.w,v.h,backdrop);
    c.rect(v.x,v.y,v.w,39,rgb(247,249,249));
    c.rect(v.x,v.y+38,v.w,1,rule);
    c.text(v.x+17,v.y+9,v.flight?"FLIGHT MODEL / SYMBOLIC HEIGHT":"ASSEMBLY / PERSPECTIVE",rgb(38,53,62));
    c.text(v.x+v.w-136,v.y+9,"Y UP  |  SI",muted);

    const int floor=v.y+v.h-55;
    const int center=v.x+v.w/2;
    const double length=r.bodyLength+r.noseLength;
    const double baseScale=std::min(1060.0,(v.h-(v.flight?190.0:130.0))/length);
    const double scale=baseScale*std::clamp(v.zoom,0.65,1.65);
    // Symbolic flight offset only: altitude is measured on the graph, never
    // against the assembly ruler. Reserve vertical room for the canopy.
    const double shift=v.flight?std::min(38.0,std::sqrt(std::max(0.0,v.altitude))*2.1):0.0;
    const int rocketBase=floor-static_cast<int>(shift);

    // The metre ruler applies only to the assembly. Flight height is compressed
    // for illustration; the chart below is the numeric altitude record.
    c.line(v.x+1,floor,v.x+v.w-1,floor,axis);
    if(!v.flight) {
        for(int n=0;n<7;++n) {
            const int tick=floor-static_cast<int>(n*0.1*scale);
            if(tick<=v.y+51)break;
            c.line(v.x+24,tick,v.x+34,tick,axis);
            if(n%2==0) {
                c.text(v.x+41,tick-12,n==0?"0":
                    (n==2?"0.2":n==4?"0.4":"0.6"),muted);
                c.line(v.x+80,tick,center-95,tick,rgb(222,229,231));
            }
        }
        c.text(v.x+24,v.y+49,"Y (m)",muted);
        c.text(v.x+18,floor+12,"XY DATUM",muted);
    } else {
        c.text(v.x+17,v.y+50,"VISUAL HEIGHT NOT TO SCALE",muted);
        c.text(v.x+18,floor+12,"GROUND",muted);
    }
    c.line(center,floor-2,center,rocketBase-static_cast<int>(length*scale)-9,
           rgb(185,199,204));
    c.line(center-86,floor+14,center+86,floor+14,rule);
    c.line(center-1,floor+4,center-1,floor+24,axis);

    const double cs=std::cos(v.yaw),sn=std::sin(v.yaw);
    const double cp=std::cos(v.pitch),sp=std::sin(v.pitch);
    auto project=[&](Point3 p)->Screen {
        const double side=p.x*cs+p.z*sn;
        const double forward=-p.x*sn+p.z*cs;
        const double height=p.y*cp+forward*sp;
        const double depth=forward*cp-p.y*sp;
        return {center+static_cast<int>(side*scale),
            rocketBase-static_cast<int>(height*scale),depth};
    };
    std::vector<Face> faces;
    faces.reserve(420);
    const auto add=[&](Point3 a,Point3 b,Point3 d,uint32_t color) {
        const Screen A=project(a),B=project(b),D=project(d);
        faces.push_back({A,B,D,color,(A.depth+B.depth+D.depth)/3.0});
    };
    const int segments=20;
    const uint32_t bodyTone=materialTone(r.material),noseTone=materialTone(r.noseMaterial);
    for(int j=0;j<12;++j) {
        const double y0=r.bodyLength*j/12.0, y1=r.bodyLength*(j+1)/12.0;
        for(int s=0;s<segments;++s) {
            const double a=2*pi*s/segments,b=2*pi*(s+1)/segments;
            const double shade=(std::cos((a+b)/2-v.yaw-0.85)+1)/2;
            const auto color=blend(rgb(83,100,111),bodyTone,
                (v.selected==0?0.27:0.10)+shade*0.63);
            Point3 A{r.radius*std::cos(a),y0,r.radius*std::sin(a)};
            Point3 B{r.radius*std::cos(b),y0,r.radius*std::sin(b)};
            Point3 C{r.radius*std::cos(a),y1,r.radius*std::sin(a)};
            Point3 D{r.radius*std::cos(b),y1,r.radius*std::sin(b)};
            add(A,B,C,color);add(B,D,C,color);
        }
    }
    for(int j=0;j<10;++j) {
        const double t0=j/10.0,t1=(j+1)/10.0;
        const double y0=r.bodyLength+t0*r.noseLength;
        const double y1=r.bodyLength+t1*r.noseLength;
        const double r0=noseRadius(r,t0),r1=noseRadius(r,t1);
        for(int s=0;s<segments;++s) {
            const double a=2*pi*s/segments,b=2*pi*(s+1)/segments;
            const double shade=(std::cos((a+b)/2-v.yaw-0.85)+1)/2;
            const auto color=blend(rgb(74,93,105),noseTone,
                (v.selected==1?0.30:0.13)+shade*0.60);
            Point3 A{r0*std::cos(a),y0,r0*std::sin(a)};
            Point3 B{r0*std::cos(b),y0,r0*std::sin(b)};
            Point3 C{r1*std::cos(a),y1,r1*std::sin(a)};
            Point3 D{r1*std::cos(b),y1,r1*std::sin(b)};
            add(A,B,C,color);add(B,D,C,color);
        }
    }
    for(int k=0;k<r.finCount;++k) {
        const double a=2*pi*k/r.finCount;
        const auto fin=[&](double rad,double y)->Point3 {
            return {rad*std::cos(a),y,rad*std::sin(a)};
        };
        const double root0=r.finSetback,root1=root0+r.finRootChord;
        const double tip0=root0+r.finSweep,tip1=tip0+r.finTipChord;
        const Point3 A=fin(r.radius,root0),B=fin(r.radius+r.finSpan,tip0);
        const Point3 C=fin(r.radius+r.finSpan,tip1),D=fin(r.radius,root1);
        const uint32_t finTone=v.selected==2?rgb(79,133,184):rgb(119,138,150);
        add(A,B,C,finTone);add(A,C,D,finTone);
    }
    std::sort(faces.begin(),faces.end(),[](const Face& a,const Face& b) {
        return a.depth>b.depth;
    });
    for(const Face& f:faces)c.triangle(f.a.x,f.a.y,f.b.x,f.b.y,f.c.x,f.c.y,f.color);

    if(v.selected==4 && !v.flight) {
        // A dashed cutaway locates the internal mount; the opaque outer shell
        // remains a visualisation, not a manufactured CAD section.
        const int topMount=project({0,r.mountLength,0}).y;
        const int sleeveHalf=std::max(4,static_cast<int>((r.mountBoreRadius+r.mountWall)*scale));
        for(int y=topMount;y<rocketBase;y+=10) {
            c.line(center-sleeveHalf,y,center-sleeveHalf,std::min(y+5,rocketBase),accent);
            c.line(center+sleeveHalf,y,center+sleeveHalf,std::min(y+5,rocketBase),accent);
        }
        c.line(center-sleeveHalf-2,topMount,center+sleeveHalf+2,topMount,accent);
        char sleeve[80];
        std::snprintf(sleeve,sizeof(sleeve),"INTERNAL MOUNT  %.0f x %.0f mm",
            r.mountLength*1000,r.mountBoreRadius*2000);
        c.text(v.x+17,v.y+76,sleeve,accent,0,340);
    }
    const int cgY=project({0,mass.cgFromTail,0}).y;
    c.line(center-80,cgY,center-22,cgY,accent);
    c.circle(center-18,cgY,3,accent);
    c.text(center-80,cgY-24,"CG",accent);
    if(v.selected==3 || v.selected==4 || v.selected==5) {
        const double position=v.selected==3?r.electronicsFromTail:
            v.selected==4?r.motorFromTail:r.recoveryFromTail;
        const int y=project({0,position,0}).y;
        c.line(center+21,y,center+71,y,accent);
        c.circle(center+18,y,3,accent);
        c.text(center+77,y-10,v.selected==3?"AVIONICS":
            v.selected==4?"MOTOR CG":"RECOVERY",accent);
    }
    const int top=project({0,length,0}).y;
    const int dimX=std::min(v.x+v.w-116,center+100);
    c.line(dimX,top,dimX,rocketBase,rgb(121,143,151));
    c.line(dimX-7,top,dimX+7,top,axis);
    c.line(dimX-7,rocketBase,dimX+7,rocketBase,axis);
    char dimension[64];
    std::snprintf(dimension,sizeof(dimension),"L %.0f mm",length*1000.0);
    c.text(dimX+12,std::max(v.y+67,(top+rocketBase)/2-10),dimension,muted,0,96);
    if(v.flight) {
        char altitude[64];
        std::snprintf(altitude,sizeof(altitude),"MODEL ALT %.1f m",v.altitude);
        c.text(v.x+v.w-215,v.y+v.h-38,altitude,accent,0,208);
        if(v.canopyOpen) {
            const int domeY=std::max(v.y+75,top-47);
            for(int i=0;i<24;++i) {
                const double a=pi*i/23.0;
                const int px=center+static_cast<int>(33*std::cos(a));
                const int py=domeY-static_cast<int>(17*std::sin(a));
                const double b=pi*(i+1)/23.0;
                c.line(px,py,center+static_cast<int>(33*std::cos(b)),
                    domeY-static_cast<int>(17*std::sin(b)),accent);
            }
            c.line(center-33,domeY,center-3,top,axis);
            c.line(center+33,domeY,center+3,top,axis);
        }
    } else {
        c.text(v.x+v.w-166,v.y+v.h-38,"DRAG / WHEEL",muted,0,158);
    }
    c.border(v.x,v.y,v.w,v.h,rule);
    c.resetClip();
}
} // namespace eulo
