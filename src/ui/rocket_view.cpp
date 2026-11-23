#include "rocket_view.hpp"
#include "core/mesh.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace eulo {
namespace {
struct Point3 { double x,y,z; };
struct Screen { int x,y; double depth; };
struct Face { Screen a,b,c; uint32_t color; double depth; };
constexpr double pi=3.14159265358979323846;
constexpr uint32_t backdrop=rgb(234,239,241),rule=rgb(206,216,220);
constexpr uint32_t muted=rgb(77,93,103),axis=rgb(121,143,151),accent=rgb(35,91,147);
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
Point3 rotate(Quaternion q,Point3 p) {
    const Point3 t{2*(q.y*p.z-q.z*p.y),2*(q.z*p.x-q.x*p.z),
                   2*(q.x*p.y-q.y*p.x)};
    return {p.x+q.w*t.x+q.y*t.z-q.z*t.y,
            p.y+q.w*t.y+q.z*t.x-q.x*t.z,
            p.z+q.w*t.z+q.x*t.y-q.y*t.x};
}
Point3 camera(Point3 p,double yaw,double pitch) {
    const double cs=std::cos(yaw),sn=std::sin(yaw);
    const double cp=std::cos(pitch),sp=std::sin(pitch);
    const double forward=-p.x*sn+p.z*cs;
    return {p.x*cs+p.z*sn,p.y*cp+forward*sp,forward*cp-p.y*sp};
}
void cameraControls(Canvas& c,const RocketView& v) {
    const char* names[]={"SIDE","TOP","ISO"};
    const int selection[]={1,2,0};
    for(int i=0;i<3;++i) {
        const int x=v.x+467+i*68;
        const bool active=v.cameraPreset==selection[i];
        c.rect(x,v.y+5,62,28,active?rgb(222,234,244):rgb(247,249,249));
        c.border(x,v.y+5,62,28,active?accent:rule);
        c.text(x+(62-c.textWidth(names[i]))/2,v.y+6,names[i],active?accent:muted);
    }
}
template<class Project>
void renderMesh(Canvas& c,const Rocket& r,int selected,double yaw,const Project& project) {
    const auto mesh=visualMesh(r);
    std::vector<Face> faces;
    faces.reserve(mesh.size());
    for(const MeshTriangle& triangle:mesh) {
        if(triangle.surface==Surface::Insert)continue; // internal; cutaway draws its location
        const double shade=(std::cos(triangle.angle-yaw-0.85)+1)*0.5;
        const uint32_t tone=triangle.surface==Surface::Body?materialTone(r.material):
            materialTone(r.noseMaterial);
        const uint32_t color=triangle.surface==Surface::Fin?
            (selected==2?rgb(79,133,184):rgb(119,138,150)):
            blend(triangle.surface==Surface::Body?rgb(83,100,111):rgb(74,93,105),
                  tone,(selected==(triangle.surface==Surface::Body?0:1)?0.30:0.12)+shade*0.62);
        const auto mapped=[&](Vec3 p) {return project({p.x,p.y,p.z});};
        const Screen A=mapped(triangle.a),B=mapped(triangle.b),D=mapped(triangle.c);
        faces.push_back({A,B,D,color,(A.depth+B.depth+D.depth)/3.0});
    }
    std::sort(faces.begin(),faces.end(),[](const Face& a,const Face& b) {
        return a.depth>b.depth;
    });
    for(const Face& f:faces)c.triangle(f.a.x,f.a.y,f.b.x,f.b.y,f.c.x,f.c.y,f.color);
}
void clippedEdge(Canvas& c,double x0,double y0,double x1,double y1,
                 const RocketView& view,uint32_t color) {
    const double dx=x1-x0,dy=y1-y0;
    const double p[]={-dx,dx,-dy,dy};
    const double q[]={x0-view.x-2,view.x+view.w-3-x0,
                      y0-view.y-40,view.y+view.h-3-y0};
    double start=0,end=1;
    for(int i=0;i<4;++i) {
        if(p[i]==0 && q[i]<0)return;
        if(p[i]==0)continue;
        const double t=q[i]/p[i];
        if(p[i]<0)start=std::max(start,t);
        else end=std::min(end,t);
        if(start>end)return;
    }
    c.line(static_cast<int>(x0+dx*start),static_cast<int>(y0+dy*start),
           static_cast<int>(x0+dx*end),static_cast<int>(y0+dy*end),color);
}
void drawReference(Canvas& c,const Rocket& r,const RocketView& v,
                   double scale,int center,int floor) {
    if(!v.reference || v.reference->triangles.empty())return;
    const uint32_t color=v.selected==8?rgb(37,111,145):rgb(115,139,148);
    const double importedScale=r.referenceUnitMm*0.001;
    const Vec3 offset{r.referenceEastMm*.001,r.referenceTailMm*.001,
                      r.referenceNorthMm*.001};
    const auto projected=[&](Vec3 p)->Point3 {
        const Point3 point{p.x*importedScale+offset.x,
                           p.y*importedScale+offset.y,
                           p.z*importedScale+offset.z};
        const Point3 cam=camera(point,v.yaw,v.pitch);
        return {center+cam.x*scale,floor-cam.y*scale,cam.z};
    };
    const size_t skip=std::max<size_t>(1,v.reference->triangles.size()/1200);
    for(size_t i=0;i<v.reference->triangles.size();i+=skip) {
        const MeshTriangle& f=v.reference->triangles[i];
        const Point3 a=projected(f.a),b=projected(f.b),d=projected(f.c);
        clippedEdge(c,a.x,a.y,b.x,b.y,v,color);
        clippedEdge(c,b.x,b.y,d.x,d.y,v,color);
        clippedEdge(c,d.x,d.y,a.x,a.y,v,color);
    }
}
void drawFlight(Canvas& c,const Rocket& r,const MassProperties& mass,const RocketView& v) {
    c.setClip(v.x,v.y,v.w,v.h);
    c.rect(v.x,v.y,v.w,v.h,backdrop);
    c.rect(v.x,v.y,v.w,39,rgb(247,249,249));
    c.rect(v.x,v.y+38,v.w,1,rule);
    c.text(v.x+17,v.y+9,"FLIGHT / SPATIAL TRACE + ATTITUDE",rgb(38,53,62));
    cameraControls(c,v);
    const int divider=v.x+428;
    c.rect(divider,v.y+39,1,v.h-39,rule);
    c.text(v.x+17,v.y+47,"PREDICTED PATH  /  METRES",muted);
    c.text(divider+16,v.y+47,"BODY ATTITUDE  /  ENLARGED",muted);
    c.text(v.x+17,v.y+74,"E east   N north   Y up",muted);
    const FlightResult& flight=*v.trajectory;
    // The trace and the body inset use different scales. Neither is a camera
    // feed or live sensor data; the world trace is fitted in metres.
    double minSide=0,maxSide=0,minHeight=0,maxHeight=0;
    const auto mapped=[&](Vec3 p) {return camera({p.x,p.y,p.z},v.yaw,v.pitch);};
    for(const FlightSample& s:flight.samples) {
        const Point3 p=mapped(s.position);
        minSide=std::min(minSide,p.x);maxSide=std::max(maxSide,p.x);
        minHeight=std::min(minHeight,p.y);maxHeight=std::max(maxHeight,p.y);
    }
    const double pathScale=std::min(360.0/(maxSide-minSide+1),
        240.0/(maxHeight-minHeight+1))*v.zoom;
    const double midSide=(minSide+maxSide)*0.5;
    const double midHeight=(minHeight+maxHeight)*0.5;
    const int plotCenterX=v.x+215,plotCenterY=v.y+248;
    const auto plot=[&](Vec3 p)->Screen {
        const Point3 w=mapped(p);
        return {plotCenterX+static_cast<int>((w.x-midSide)*pathScale),
                plotCenterY-static_cast<int>((w.y-midHeight)*pathScale),w.z};
    };
    const Screen origin=plot({});
    const double unit=std::max(5.0,std::min(50.0,
        std::max(flight.apogee,flight.maxRange)*0.12));
    const Screen east=plot({unit,0,0}),north=plot({0,0,unit}),up=plot({0,unit,0});
    c.line(origin.x,origin.y,east.x,east.y,axis);
    c.line(origin.x,origin.y,north.x,north.y,axis);
    c.line(origin.x,origin.y,up.x,up.y,axis);
    c.circle(origin.x,origin.y,3,axis);
    c.text(east.x+3,east.y-12,"E",muted);
    c.text(north.x+3,north.y-12,"N",muted);
    c.text(up.x+3,up.y-12,"Y",muted);
    const int skip=std::max(1,static_cast<int>(flight.samples.size()/650));
    Screen previous=plot(flight.samples.front().position);
    Screen previousGround=plot({flight.samples.front().position.x,0,
                                flight.samples.front().position.z});
    for(size_t i=static_cast<size_t>(skip);i<flight.samples.size();i+=static_cast<size_t>(skip)) {
        const Vec3 pos=flight.samples[i].position;
        const Screen next=plot(pos),ground=plot({pos.x,0,pos.z});
        c.line(previousGround.x,previousGround.y,ground.x,ground.y,
               rgb(159,173,180));
        c.line(previous.x,previous.y,next.x,next.y,accent,2);
        previous=next;previousGround=ground;
    }
    const Screen end=plot(flight.samples.back().position);
    c.line(previous.x,previous.y,end.x,end.y,accent,2);
    const Screen location=plot(v.sample.position);
    const Screen ground=plot({v.sample.position.x,0,v.sample.position.z});
    for(int n=0;n<8;++n) {
        const double f0=n/8.0,f1=std::min(1.0,f0+0.065);
        c.line(ground.x+static_cast<int>((location.x-ground.x)*f0),
               ground.y+static_cast<int>((location.y-ground.y)*f0),
               ground.x+static_cast<int>((location.x-ground.x)*f1),
               ground.y+static_cast<int>((location.y-ground.y)*f1),rule);
    }
    c.circle(location.x,location.y,6,accent);
    c.circle(location.x,location.y,2,rgb(255,255,255));
    c.text(v.x+17,v.y+v.h-62,"BLUE  flight    GREY  ground track",muted);
    char scaleText[64];
    std::snprintf(scaleText,sizeof(scaleText),"AXIS MARK %.0f m  /  orbit + zoom",unit);
    c.text(v.x+17,v.y+v.h-35,scaleText,muted,0,400);

    c.rect(divider+1,v.y+84,v.w-(divider-v.x)-2,v.h-162,rgb(239,243,244));
    const int modelX=divider+(v.x+v.w-divider)/2;
    const int modelY=v.y+253;
    c.line(modelX,modelY-126,modelX,modelY+126,rule);
    c.line(divider+28,modelY,divider+v.w-(divider-v.x)-27,modelY,rule);
    c.text(divider+16,v.y+91,"Y UP",muted);
    const double bodyScale=215.0/(r.bodyLength+r.noseLength)*v.zoom;
    const auto body=[&](Point3 point)->Screen {
        const Point3 centered{point.x,point.y-mass.cgFromTail,point.z};
        const Point3 world=rotate(v.sample.orientation,centered);
        const Point3 p=camera(world,v.yaw,v.pitch);
        return {modelX+static_cast<int>(p.x*bodyScale),
                modelY-static_cast<int>(p.y*bodyScale),p.z};
    };
    renderMesh(c,r,-1,v.yaw,body);
    const Screen center=body({0,mass.cgFromTail,0});
    c.circle(center.x,center.y,4,accent);
    c.text(divider+17,v.y+v.h-115,"CG / model pivot",accent);
    if(v.sample.canopyOpen && r.parachuteDiameter>0) {
        c.text(divider+17,v.y+v.h-83,"Ideal canopy drag ON",muted);
    } else if(v.sample.thrust>0) {
        c.text(divider+17,v.y+v.h-83,"Open-loop gimbal / thrust ON",muted);
    }
    char reading[96];
    std::snprintf(reading,sizeof(reading),"TILT %.1f deg    RANGE %.1f m",
                  v.sample.tiltDeg,v.sample.range);
    c.text(divider+17,v.y+v.h-40,reading,accent,0,v.w-(divider-v.x)-28);
    c.border(v.x,v.y,v.w,v.h,rule);
    c.resetClip();
}
} // namespace

void drawRocketView(Canvas& c,const Rocket& r,const MassProperties& mass,const RocketView& v) {
    if(v.flight && v.trajectory && v.trajectory->valid && !v.trajectory->samples.empty()) {
        drawFlight(c,r,mass,v);return;
    }
    c.setClip(v.x,v.y,v.w,v.h);
    c.rect(v.x,v.y,v.w,v.h,backdrop);
    c.rect(v.x,v.y,v.w,39,rgb(247,249,249));
    c.rect(v.x,v.y+38,v.w,1,rule);
    c.text(v.x+17,v.y+9,"ASSEMBLY / DIMENSIONAL VIEW",rgb(38,53,62));
    cameraControls(c,v);
    const int floor=v.y+v.h-55;
    const int center=v.x+v.w/2;
    const double length=r.bodyLength+r.noseLength;
    const double scale=std::min(1060.0,(v.h-130.0)/length)*v.zoom;
    c.line(v.x+1,floor,v.x+v.w-1,floor,axis);
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
    c.line(center,floor-2,center,floor-static_cast<int>(length*scale)-9,rgb(185,199,204));
    c.line(center-86,floor+14,center+86,floor+14,rule);
    c.line(center-1,floor+4,center-1,floor+24,axis);
    const auto project=[&](Point3 point)->Screen {
        const Point3 p=camera(point,v.yaw,v.pitch);
        return {center+static_cast<int>(p.x*scale),
                floor-static_cast<int>(p.y*scale),p.z};
    };
    renderMesh(c,r,v.selected,v.yaw,project);
    drawReference(c,r,v,scale,center,floor);
    if(v.selected==8) {
        if(v.reference && !v.reference->triangles.empty()) {
            char label[96];
            std::snprintf(label,sizeof(label),"REFERENCE WIREFRAME / %zu FACES / NO MASS",
                          v.reference->triangles.size());
            c.text(v.x+17,v.y+76,label,accent,0,410);
        } else c.text(v.x+17,v.y+76,"NO REFERENCE MESH / IMPORT OBJ OR STL",muted,0,410);
    }
    if(v.selected==4) {
        const int topMount=project({0,r.mountLength,0}).y;
        const int half=std::max(4,static_cast<int>((r.mountBoreRadius+r.mountWall)*scale));
        for(int y=topMount;y<floor;y+=10) {
            c.line(center-half,y,center-half,std::min(y+5,floor),accent);
            c.line(center+half,y,center+half,std::min(y+5,floor),accent);
        }
        c.line(center-half-2,topMount,center+half+2,topMount,accent);
        char label[80];
        std::snprintf(label,sizeof(label),"INTERNAL MOUNT  %.0f x %.0f mm",
                      r.mountLength*1000,r.mountBoreRadius*2000);
        c.text(v.x+17,v.y+76,label,accent,0,340);
    }
    if(v.selected==6) {
        if(!r.insertEnabled)c.text(v.x+17,v.y+76,"INSERT NOT BUILT / enable in properties",accent);
        else {
            const int top=project({0,r.insertFromTail+r.insertLength/2,0}).y;
            const int bottom=project({0,r.insertFromTail-r.insertLength/2,0}).y;
            const int half=std::max(4,static_cast<int>(r.insertOuterRadius*scale));
            for(int y=top;y<bottom;y+=8) {
                c.line(center-half,y,center-half,std::min(bottom,y+4),accent);
                c.line(center+half,y,center+half,std::min(bottom,y+4),accent);
            }
            c.line(center-half,top,center+half,top,accent);
            c.line(center-half,bottom,center+half,bottom,accent);
            char label[80];
            std::snprintf(label,sizeof(label),"MODEL INSERT / %.1f x %.1f mm",
                          r.insertLength*1000,r.insertOuterRadius*2000);
            c.text(v.x+17,v.y+76,label,accent,0,360);
        }
    }
    if(v.selected==7) {
        const int cpY=project({0,r.cpFromTail,0}).y;
        c.line(center+19,cpY,center+69,cpY,accent);
        c.text(center+76,cpY-12,"CP*",accent);
        c.text(v.x+17,v.y+76,"CP* is a scenario assumption, not measured",muted);
    }
    const int cgY=project({0,mass.cgFromTail,0}).y;
    c.line(center-80,cgY,center-22,cgY,accent);
    c.circle(center-18,cgY,3,accent);
    c.text(center-80,cgY-24,"CG*",accent);
    if(v.selected==3 || v.selected==4 || v.selected==5 ||
       (v.selected==6 && r.insertEnabled)) {
        const double at=v.selected==3?r.electronicsFromTail:
            v.selected==4?r.motorFromTail:v.selected==5?r.recoveryFromTail:r.insertFromTail;
        const int y=project({0,at,0}).y;
        c.line(center+21,y,center+71,y,accent);
        c.circle(center+18,y,3,accent);
        c.text(center+77,y-10,v.selected==3?"AVIONICS":
            v.selected==4?"MOTOR CG":v.selected==5?"RECOVERY":"INSERT CG",accent);
    }
    const int top=project({0,length,0}).y;
    const int dimX=std::min(v.x+v.w-116,center+100);
    c.line(dimX,top,dimX,floor,rgb(121,143,151));
    c.line(dimX-7,top,dimX+7,top,axis);
    c.line(dimX-7,floor,dimX+7,floor,axis);
    char dimension[64];
    std::snprintf(dimension,sizeof(dimension),"L %.0f mm",length*1000.0);
    c.text(dimX+12,std::max(v.y+67,(top+floor)/2-10),dimension,muted,0,96);
    c.text(v.x+v.w-166,v.y+v.h-38,"DRAG / WHEEL",muted,0,158);
    c.border(v.x,v.y,v.w,v.h,rule);
    c.resetClip();
}

void drawRecordedView(Canvas& c,const RocketView& v,
                      const TelemetrySample& sample,bool calibrated,
                      double relativeAltitudeM,bool hasPressure) {
    c.setClip(v.x,v.y,v.w,v.h);
    c.rect(v.x,v.y,v.w,v.h,backdrop);
    c.rect(v.x,v.y,v.w,39,rgb(247,249,249));
    c.rect(v.x,v.y+38,v.w,1,rule);
    c.text(v.x+17,v.y+9,"GROUND / RECORDED SENSOR DATA",rgb(38,53,62));
    if(calibrated && sample.attitudeValid)cameraControls(c,v);
    c.text(v.x+17,v.y+52,"CSV PLAYBACK  /  DEVICE ORIGIN NOT VERIFIED",muted);
    if(!calibrated) {
        c.rect(v.x+37,v.y+153,v.w-74,1,rule);
        c.text(v.x+39,v.y+179,"No relative attitude or altitude reference",rgb(38,53,62),1);
        c.text(v.x+39,v.y+230,"Record the first two seconds with the IMU stationary.",muted);
        c.text(v.x+39,v.y+262,"Calibrate in the inspector to set the initial reference.",muted);
        c.text(v.x+39,v.y+294,"Raw CSV readings are shown without a vehicle model.",muted);
    } else {
        const int centerX=v.x+v.w/2+36,centerY=v.y+255;
        if(sample.attitudeValid) {
            c.line(centerX,centerY-147,centerX,centerY+143,rule);
            c.line(centerX-147,centerY,centerX+147,centerY,rule);
            // A rotating body-axis triad represents integrated sensor rates.
            // Drawing the project's rocket here would falsely imply a measured
            // vehicle shape, installation orientation or actual fin travel.
            const auto project=[&](Point3 point)->Screen {
                const Point3 p=camera(rotate(sample.attitude,point),v.yaw,v.pitch);
                const double scale=125.0*v.zoom;
                return {centerX+static_cast<int>(p.x*scale),
                        centerY-static_cast<int>(p.y*scale),p.z};
            };
            const Screen origin=project({0,0,0});
            const Screen x=project({1,0,0}),y=project({0,1,0}),z=project({0,0,1});
            c.line(origin.x,origin.y,x.x,x.y,rgb(128,152,163),3);
            c.line(origin.x,origin.y,y.x,y.y,accent,3);
            c.line(origin.x,origin.y,z.x,z.y,rgb(96,117,124),3);
            c.circle(origin.x,origin.y,5,accent);
            c.text(x.x+5,x.y-11,"X",muted);
            c.text(y.x+5,y.y-11,"Y",accent);
            c.text(z.x+5,z.y-11,"Z",muted);
        } else c.text(v.x+70,v.y+215,
            "Orientation unavailable after a sample gap",rgb(38,53,62),1);
        c.rect(v.x+18,v.y+330,315,1,rule);
        c.text(v.x+18,v.y+344,"SENSOR AXES / RELATIVE TO FIRST SAMPLE",muted,0,390);
        c.text(v.x+18,v.y+376,"Gyro-only pose drifts; no GPS or position.",muted,0,385);
        c.text(v.x+18,v.y+407,"No measured rocket shape or actuator travel.",muted,0,385);
        char altitude[80];
        std::snprintf(altitude,sizeof(altitude),"%s  %+.1f m",
                      hasPressure?"BARO DELTA":"CSV ALT DELTA",relativeAltitudeM);
        c.text(v.x+v.w-280,v.y+v.h-41,altitude,accent,0,266);
    }
    c.border(v.x,v.y,v.w,v.h,rule);
    c.resetClip();
}
} // namespace eulo
