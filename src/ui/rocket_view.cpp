#include "rocket_view.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

namespace eulo {
namespace {
struct Point3 { double x,y,z; };
struct Screen { int x,y; double depth; };
struct Face { Screen a,b,c; uint32_t color; double depth; };
constexpr double pi=3.141592653589793;
uint32_t mix(uint32_t a,uint32_t b,double t) {
    t=std::clamp(t,0.0,1.0);
    const auto m=[t](uint32_t p,uint32_t q){return static_cast<int>(p*(1.0-t)+q*t);};
    return rgb(m((a>>16)&255,(b>>16)&255),m((a>>8)&255,(b>>8)&255),m(a&255,b&255));
}
void label(Canvas& c,int x,int y,const char* text,uint32_t color) {
    c.text(x,y,text,color,0);
}
}

void drawRocketView(Canvas& c,const Rocket& rocket,const MassProperties& mass,const RocketView& v) {
    const auto bg0=rgb(12,27,41),bg1=rgb(8,18,29);
    c.gradient(v.x,v.y,v.w,v.h,bg0,bg1);
    // Deterministic fine-grain backdrop and altitude-grid: purely visual.
    for(int i=0;i<55;++i) {
        const int px=v.x+17+(i*139+17)%std::max(30,v.w-30);
        const int py=v.y+36+(i*71+11)%std::max(30,v.h-70);
        c.rect(px,py,i%8==0?2:1,i%8==0?2:1,rgb(34,61,72));
    }
    for(int k=0;k<=8;++k) {
        const int gy=v.y+v.h-39-k*48;
        c.line(v.x+1,gy,v.x+v.w-1,gy,rgb(24,45,56));
        label(c,v.x+13,gy-24,(k%2==0?"- - -":""),rgb(45,73,81));
    }
    const int floor=v.y+v.h-47;
    c.gradient(v.x,floor,v.w,47,rgb(25,41,46),rgb(17,30,35));
    c.rect(v.x,floor,v.w,2,rgb(93,124,122));
    for(int i=0;i<8;++i) {
        c.line(v.x+v.w/2,floor,v.x+(i-2)*128,floor+47,rgb(37,62,63));
    }
    const int center=v.x+v.w/2;
    const int groundY=floor-7;
    const int rocketBase=v.flight?groundY-std::min(125,static_cast<int>(std::sqrt(std::max(0.0,v.altitude))*18)):groundY;
    if(v.flight) {
        for(int i=0;i<14;++i) {
            const int pathY=groundY-i*29;
            c.circle(center+(i%3-1)*3,pathY,1,rgb(135,115,61));
        }
    }
    // Launch rail, base and a subtle shadow.
    c.circle(center+32,groundY+4,43,rgb(12,23,27));
    c.line(center+37,groundY,center+37,groundY-339,rgb(80,106,107),2);
    c.line(center+40,groundY,center+40,groundY-339,rgb(33,62,68));
    c.rect(center-90,groundY+3,175,6,rgb(72,87,82));
    c.line(center,groundY,center,rocketBase-(v.flight?220:405),rgb(41,75,79));

    const double scale=std::min(v.flight?440.0:730.0,
        static_cast<double>(v.h-140)/(rocket.bodyLength+rocket.noseLength));
    const double cs=std::cos(v.yaw),sn=std::sin(v.yaw);
    auto project=[&](Point3 p)->Screen {
        const double side=p.x*cs+p.z*sn;
        const double depth=-p.x*sn+p.z*cs;
        return {center+static_cast<int>(side*scale),
                rocketBase-static_cast<int>((p.y+depth*0.12)*scale),depth};
    };
    std::vector<Face> faces;
    faces.reserve(256);
    auto add=[&](Point3 a,Point3 b,Point3 d,uint32_t color) {
        const Screen A=project(a),B=project(b),D=project(d);
        faces.push_back({A,B,D,color,(A.depth+B.depth+D.depth)/3.0});
    };
    const int segments=20;
    const uint32_t bodyBase=rocket.material==Material::PVC?rgb(224,231,227):
        (rocket.material==Material::Aluminium?rgb(184,204,214):rgb(70,89,100));
    const uint32_t dark=rocket.material==Material::CarbonLaminate?rgb(27,42,51):rgb(72,92,102);
    auto bodyRing=[&](double y,double step,int band) {
        for(int s=0;s<segments;++s) {
            const double a=2*pi*s/segments,b=2*pi*(s+1)/segments;
            const double shade=std::clamp(0.22+0.75*(std::cos((a+b)/2-v.yaw-0.8)+1)/2,0.0,1.0);
            const uint32_t tint=band?mix(rgb(15,37,46),rgb(52,74,82),shade):mix(dark,bodyBase,shade);
            Point3 p{rocket.radius*std::cos(a),y,rocket.radius*std::sin(a)},
                   q{rocket.radius*std::cos(b),y,rocket.radius*std::sin(b)},
                   t{rocket.radius*std::cos(a),y+step,rocket.radius*std::sin(a)},
                   u{rocket.radius*std::cos(b),y+step,rocket.radius*std::sin(b)};
            add(p,q,t,tint);add(q,u,t,tint);
        }
    };
    bodyRing(0,0.028,1);
    bodyRing(0.028,rocket.bodyLength-0.09,0);
    bodyRing(rocket.bodyLength-0.062,0.028,1);
    bodyRing(rocket.bodyLength-0.034,0.028,0);
    bodyRing(rocket.bodyLength-0.006,0.006,1);
    // Nose rings, approximated by a conic profile. Geometry reacts to UI edits.
    for(int j=0;j<8;++j) {
        const double t0=static_cast<double>(j)/8,t1=static_cast<double>(j+1)/8;
        const double y0=rocket.bodyLength+t0*rocket.noseLength;
        const double y1=rocket.bodyLength+t1*rocket.noseLength;
        const double r0=rocket.radius*(1-t0),r1=rocket.radius*(1-t1);
        for(int s=0;s<segments;++s) {
            const double a=2*pi*s/segments,b=2*pi*(s+1)/segments;
            const double shade=std::clamp(0.4+0.4*std::cos((a+b)/2-v.yaw-0.8),0.0,1.0);
            const uint32_t col=mix(rgb(56,87,91),rgb(216,238,221),shade);
            Point3 A{r0*std::cos(a),y0,r0*std::sin(a)},B{r0*std::cos(b),y0,r0*std::sin(b)};
            Point3 C{r1*std::cos(a),y1,r1*std::sin(a)},D{r1*std::cos(b),y1,r1*std::sin(b)};
            add(A,B,C,col);add(B,D,C,col);
        }
    }
    // Three planar fins with extruded visual face and colored edges.
    for(int k=0;k<3;++k) {
        const double a=2*pi*k/3;
        const auto fin=[&](double radius,double y)->Point3 {
            return {radius*std::cos(a),y,radius*std::sin(a)};
        };
        const Point3 A=fin(rocket.radius,0.035),B=fin(rocket.radius+rocket.finSpan,0.006);
        const Point3 C=fin(rocket.radius+rocket.finSpan*.85,0.070),D=fin(rocket.radius,0.145);
        const uint32_t col=k==v.selected?rgb(255,183,95):rgb(194,128,72);
        add(A,B,C,col);add(A,C,D,col);
    }
    std::sort(faces.begin(),faces.end(),[](const Face& a,const Face& b){return a.depth>b.depth;});
    for(const auto& face:faces) c.triangle(face.a.x,face.a.y,face.b.x,face.b.y,
                                            face.c.x,face.c.y,face.color);
    // An actual CG marker computed from the same model as the simulator.
    const int cgY=project({0,mass.cgFromTail,0}).y;
    c.line(center-90,cgY,center-39,cgY,rgb(68,191,187));
    c.circle(center-40,cgY,4,rgb(68,191,187));
    label(c,center-159,cgY-15,"CG",rgb(105,210,204));
    if(v.thrust>0 && v.flight) {
        const int flame=std::min(55,17+static_cast<int>(v.thrust*0.5));
        c.triangle(center-10,rocketBase,center+10,rocketBase,center,rocketBase+flame,rgb(230,114,51));
        c.triangle(center-5,rocketBase,center+5,rocketBase,center,rocketBase+flame*2/3,rgb(255,220,119));
    }
    if(v.canopyOpen) {
        const int tip=project({0,rocket.bodyLength+rocket.noseLength,0}).y;
        const int top=tip-52;
        for(int y=0;y<=25;++y) {
            const double q=1.0-(25.0-y)*(25.0-y)/(25.0*25.0);
            const int half=static_cast<int>(47*std::sqrt(q));
            c.rect(center-half,top+y,half*2+1,1,
                mix(rgb(192,91,62),rgb(255,182,103),static_cast<double>(y)/25));
        }
        c.line(center-45,top+25,center-3,tip,rgb(207,190,159));
        c.line(center+45,top+25,center+3,tip,rgb(207,190,159));
        c.line(center,top+15,center,tip,rgb(207,190,159));
    }
    label(c,v.x+23,v.y+16,v.flight?"SIMULATED FLIGHT / 1D MODEL":"ASSEMBLY VIEW / PARAMETRIC",rgb(127,191,190));
    label(c,v.x+23,v.y+46,v.flight?"CAMERA / NO SCALE":"ROTATING 3D MESH  /  CG MARKER",rgb(91,119,128));
    c.border(v.x,v.y,v.w,v.h,rgb(35,62,69));
}
} // namespace eulo
