// Approximate 3-D translational and rotational experiment with an open-loop
// thrust vector and estimated fin sideforce. NO validated coefficients,
// feedback guidance, servo communication, or real-flight safety use.
#include "flight.hpp"
#include <algorithm>
#include <cmath>

namespace eulo {
namespace {
constexpr double pi=3.14159265358979323846;
constexpr double deg=180.0/pi;
constexpr double gravity=9.80665;
constexpr double seaRho=1.225;
constexpr double atmosphereH=8500.0;
constexpr double dt=0.005;
constexpr double railLength=1.0;
Vec3 operator+(Vec3 a,Vec3 b) {return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 operator-(Vec3 a,Vec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 operator*(Vec3 a,double n) {return {a.x*n,a.y*n,a.z*n};}
Vec3 operator/(Vec3 a,double n) {return a*(1/n);}
double dot(Vec3 a,Vec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double length(Vec3 a) {return std::sqrt(dot(a,a));}
Vec3 unit(Vec3 a) {const double n=length(a);return n>1e-12?a/n:Vec3{};}
Quaternion multiply(Quaternion a,Quaternion b) {
    return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
        a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};
}
Quaternion normalized(Quaternion q) {
    const double size=std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);
    return size>1e-12?Quaternion{q.w/size,q.x/size,q.y/size,q.z/size}:Quaternion{};
}
Vec3 rotate(Quaternion q,Vec3 v) {
    const Vec3 u{q.x,q.y,q.z};
    const Vec3 t=cross(u,v)*2.0;
    return v+t*q.w+cross(u,t);
}
Quaternion toAxis(Vec3 axis) {
    const Vec3 u{0,1,0};
    const Vec3 c=cross(u,axis);
    return normalized({1.0+dot(u,axis),c.x,c.y,c.z});
}
Quaternion integrate(Quaternion orientation,Vec3 omega,double step) {
    const double speed=length(omega);
    if(speed<1e-12)return orientation;
    const double half=speed*step*0.5;
    const Vec3 v=omega*(std::sin(half)/speed);
    return normalized(multiply({std::cos(half),v.x,v.y,v.z},orientation));
}
double speedOfSound(double altitude) {
    const double kelvin=std::max(216.65,288.15-0.0065*std::max(0.0,altitude));
    return std::sqrt(1.4*287.05*kelvin);
}
struct State { Vec3 pos,vel,omega;Quaternion q; };
struct Dynamics {
    Vec3 acceleration,angularAcceleration;
    double thrust,rho,remaining,airSpeed,properG;
};
} // namespace

bool motorFits(const Rocket& rocket,const MotorCurve& motor) {
    const double halfLength=motor.lengthMm*0.0005;
    return rocket.valid() && std::isfinite(motor.diameterMm) &&
        std::isfinite(motor.lengthMm) && motor.diameterMm>0 && motor.lengthMm>0 &&
        motor.diameterMm*0.0005 <= rocket.mountBoreRadius &&
        rocket.motorFromTail >= halfLength-1e-9 &&
        rocket.motorFromTail+halfLength <= rocket.mountLength+1e-9;
}

FlightResult simulate(const Rocket& r,const MotorCurve& motor,double initialAltitude) {
    FlightResult out;
    if(!motorFits(r,motor) || motor.points.size()<3 || motor.totalImpulse()<=0 ||
       !std::isfinite(initialAltitude) || initialAltitude<0 || initialAltitude>10000 ||
       motor.loadedKg<=motor.propellantKg)return out;
    const MassProperties full=massProperties(r,motor.loadedKg,motor.propellantKg);
    const MassProperties spent=massProperties(r,motor.loadedKg-motor.propellantKg,0);
    if(full.dry<=0 || full.momentPitch<=0 || full.momentRoll<=0)return out;
    const double airframeArea=pi*r.radius*r.radius;
    const double canopyArea=pi*r.parachuteDiameter*r.parachuteDiameter/4.0;
    const Vec3 wind{r.windEast,0,r.windNorth};
    const double pitch=r.gimbalPitchDeg/deg,yaw=r.gimbalYawDeg/deg;
    const Vec3 nozzle=unit({std::tan(pitch),1,std::tan(yaw)});
    const double tilt=r.launchTiltDeg/deg,heading=r.launchHeadingDeg/deg;
    const Vec3 initialAxis{std::sin(tilt)*std::cos(heading),std::cos(tilt),
                           std::sin(tilt)*std::sin(heading)};
    State s{{0,initialAltitude,0},{},{},toAxis(initialAxis)};
    bool airborne=initialAltitude>0,railReleased=initialAltitude>=railLength,canopy=false;
    out.valid=true;out.apogee=initialAltitude;
    out.samples.reserve(6500);
    auto evaluate=[&](const State& state,double t,bool deployed)->Dynamics {
        const double fraction=std::clamp(motor.impulse(t)/motor.totalImpulse(),0.0,1.0);
        const double remaining=motor.propellantKg*(1.0-fraction);
        const double mass=full.dry+remaining;
        const double rho=seaRho*std::exp(-std::max(0.0,state.pos.y)/atmosphereH);
        const Vec3 relative=state.vel-wind;
        const double airSpeed=length(relative);
        const double q=0.5*rho*airSpeed*airSpeed;
        const Vec3 axis=rotate(state.q,{0,1,0});
        const Vec3 thrust=rotate(state.q,nozzle)*motor.thrust(t);
        const Vec3 drag=relative*(-0.5*rho*airSpeed*(r.airframeCd*airframeArea+
            (deployed?r.canopyCd*canopyArea:0.0)));
        // Simplified fin normal-force slope (dimensionless), not a measured CP.
        const Vec3 transverse=relative-axis*dot(relative,axis);
        const Vec3 fin=airSpeed>1e-6 && !deployed?
            transverse*(-q*airframeArea*2.0/airSpeed):Vec3{};
        const Vec3 force=thrust+drag+fin;
        const Vec3 acceleration=force/mass+Vec3{0,-gravity,0};
        const double cg=full.cgFromTail+(spent.cgFromTail-full.cgFromTail)*fraction;
        const Vec3 torque=cross(axis*(r.motorFromTail-cg),thrust)+
            cross(axis*(r.cpFromTail-cg),fin)-state.omega*r.angularDamping;
        const double pitchInertia=std::max(1e-7,full.momentPitch+
            (spent.momentPitch-full.momentPitch)*fraction);
        const double rollInertia=std::max(1e-7,full.momentRoll+
            (spent.momentRoll-full.momentRoll)*fraction);
        const Vec3 parallel=axis*dot(state.omega,axis);
        const Vec3 angularMomentum=parallel*rollInertia+
            (state.omega-parallel)*pitchInertia;
        const Vec3 corrected=torque-cross(state.omega,angularMomentum);
        const Vec3 angularAcceleration=(corrected-axis*dot(corrected,axis))/pitchInertia+
            axis*(dot(corrected,axis)/rollInertia);
        return {acceleration,angularAcceleration,motor.thrust(t),rho,
            remaining,airSpeed,length(force)/(mass*gravity)};
    };
    auto advance=[](State state,const Dynamics& f,double step)->State {
        state.pos=state.pos+state.vel*step+f.acceleration*(0.5*step*step);
        state.q=integrate(state.q,state.omega+f.angularAcceleration*(step*0.5),step);
        state.vel=state.vel+f.acceleration*step;
        state.omega=state.omega+f.angularAcceleration*step;
        return state;
    };
    auto sample=[&](const State& state,double t,bool deployed)->FlightSample {
        const Dynamics f=evaluate(state,t,deployed);
        const Vec3 axis=rotate(state.q,{0,1,0});
        FlightSample result;
        result.time=t;result.altitude=state.pos.y;result.velocity=state.vel.y;
        result.acceleration=state.pos.y<=0 && state.vel.y<=0 &&
            f.acceleration.y<0?0:f.acceleration.y;
        result.thrust=f.thrust;result.properG=f.properG;
        result.dynamicPressure=0.5*f.rho*f.airSpeed*f.airSpeed;
        result.propellantRemaining=f.remaining;
        result.position=state.pos;result.velocityWorld=state.vel;
        result.angularVelocity=state.omega;result.orientation=state.q;
        result.speed=length(state.vel);
        result.mach=f.airSpeed/speedOfSound(state.pos.y);
        result.range=std::hypot(state.pos.x,state.pos.z);
        result.tiltDeg=std::acos(std::clamp(axis.y,-1.0,1.0))*deg;
        result.headingDeg=std::atan2(axis.z,axis.x)*deg;
        result.canopyOpen=deployed;
        return result;
    };
    for(int i=0;i<=60000;++i) {
        const double t=i*dt;
        if(airborne && s.vel.y<0 && r.parachuteDiameter>0)canopy=true;
        const FlightSample current=sample(s,t,canopy);
        if(i%10==0 || i==60000)out.samples.push_back(current);
        if(current.altitude>out.apogee) {out.apogee=current.altitude;out.apogeeTime=t;}
        out.maxSpeed=std::max(out.maxSpeed,current.speed);
        out.maxG=std::max(out.maxG,current.properG);
        out.maxQ=std::max(out.maxQ,current.dynamicPressure);
        out.maxMach=std::max(out.maxMach,current.mach);
        out.maxRange=std::max(out.maxRange,current.range);
        out.maxTiltDeg=std::max(out.maxTiltDeg,current.tiltDeg);
        if(!airborne && t>motor.burnTime()+0.5)break;
        const Dynamics f=evaluate(s,t,canopy);
        if(!airborne && f.acceleration.y<=0)continue;
        const State midpoint=advance(s,f,dt*0.5);
        const Dynamics mid=evaluate(midpoint,t+dt*0.5,canopy);
        State next=advance(s,mid,dt);
        if(!railReleased) {
            const double distance=std::max(0.0,dot(next.pos-Vec3{0,initialAltitude,0},initialAxis));
            const double along=std::max(0.0,dot(next.vel,initialAxis));
            next.pos=Vec3{0,initialAltitude,0}+initialAxis*distance;
            next.vel=initialAxis*along;
            next.q=toAxis(initialAxis);next.omega={};
            if(distance>=railLength)railReleased=true;
        }
        if(!std::isfinite(next.pos.x) || !std::isfinite(next.pos.y) ||
           !std::isfinite(next.pos.z) || !std::isfinite(next.vel.x) ||
           !std::isfinite(next.vel.y) || !std::isfinite(next.vel.z) ||
           !std::isfinite(next.omega.x) || !std::isfinite(next.omega.y) ||
           !std::isfinite(next.omega.z)) {
            out.valid=false;out.samples.clear();return out;
        }
        if(next.pos.y>0)airborne=true;
        if(s.vel.y>0 && next.vel.y<=0 && s.pos.y>=out.apogee-1.0) {
            const double fap=std::clamp(s.vel.y/(s.vel.y-next.vel.y),0.0,1.0);
            out.apogee=s.pos.y+(next.pos.y-s.pos.y)*fap;
            out.apogeeTime=t+dt*fap;
        }
        if(next.pos.y<=0 && airborne) {
            const double fground=std::clamp(s.pos.y/(s.pos.y-next.pos.y),0.0,1.0);
            const Vec3 before=s.vel+(next.vel-s.vel)*fground;
            out.touchdownSpeed=length(before);
            out.maxSpeed=std::max(out.maxSpeed,out.touchdownSpeed);
            State landed=next;
            landed.pos=s.pos+(next.pos-s.pos)*fground;landed.pos.y=0;
            landed.vel={};landed.omega={};
            out.landed=true;
            out.samples.push_back(sample(landed,t+dt*fground,canopy));
            break;
        }
        s=next;
    }
    return out;
}

FlightSample FlightResult::at(double time) const {
    if(samples.empty())return {};
    if(time<=samples.front().time)return samples.front();
    if(time>=samples.back().time)return samples.back();
    const auto it=std::lower_bound(samples.begin(),samples.end(),time,
        [](const FlightSample& s,double t){return s.time<t;});
    const FlightSample& a=*(it-1),&b=*it;
    const double fraction=(time-a.time)/(b.time-a.time);
    auto linear=[fraction](double x,double y){return x+(y-x)*fraction;};
    FlightSample result;
    result.time=time;
    result.altitude=linear(a.altitude,b.altitude);
    result.velocity=linear(a.velocity,b.velocity);
    result.acceleration=linear(a.acceleration,b.acceleration);
    result.thrust=linear(a.thrust,b.thrust);
    result.properG=linear(a.properG,b.properG);
    result.dynamicPressure=linear(a.dynamicPressure,b.dynamicPressure);
    result.propellantRemaining=linear(a.propellantRemaining,b.propellantRemaining);
    result.position={linear(a.position.x,b.position.x),result.altitude,
                     linear(a.position.z,b.position.z)};
    result.velocityWorld={linear(a.velocityWorld.x,b.velocityWorld.x),
                          result.velocity,linear(a.velocityWorld.z,b.velocityWorld.z)};
    result.angularVelocity={linear(a.angularVelocity.x,b.angularVelocity.x),
                            linear(a.angularVelocity.y,b.angularVelocity.y),
                            linear(a.angularVelocity.z,b.angularVelocity.z)};
    Quaternion qb=b.orientation;
    if(a.orientation.w*qb.w+a.orientation.x*qb.x+a.orientation.y*qb.y+
       a.orientation.z*qb.z<0)qb={-qb.w,-qb.x,-qb.y,-qb.z};
    result.orientation=normalized({linear(a.orientation.w,qb.w),
        linear(a.orientation.x,qb.x),linear(a.orientation.y,qb.y),
        linear(a.orientation.z,qb.z)});
    result.speed=length(result.velocityWorld);
    result.mach=linear(a.mach,b.mach);
    result.range=std::hypot(result.position.x,result.position.z);
    const Vec3 axis=rotate(result.orientation,{0,1,0});
    result.tiltDeg=std::acos(std::clamp(axis.y,-1.0,1.0))*deg;
    result.headingDeg=std::atan2(axis.z,axis.x)*deg;
    result.canopyOpen=fraction<0.5?a.canopyOpen:b.canopyOpen;
    return result;
}
} // namespace eulo
