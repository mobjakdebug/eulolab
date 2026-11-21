#include "telemetry.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>

namespace eulo {
namespace {
constexpr double rad=3.14159265358979323846/180.0;
constexpr const char* columns="time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s";
Quaternion product(Quaternion a,Quaternion b) {
    return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
        a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
        a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
        a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};
}
Quaternion unit(Quaternion q) {
    const double n=std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);
    return n>1e-12?Quaternion{q.w/n,q.x/n,q.y/n,q.z/n}:Quaternion{};
}
bool row(const std::string& line,TelemetrySample& sample) {
    const char* next=line.c_str();
    double values[5]={};
    for(int i=0;i<5;++i) {
        char* end=nullptr;errno=0;
        values[i]=std::strtod(next,&end);
        if(next==end || !std::isfinite(values[i]) || errno==ERANGE ||
           (i<4?*end!=',':*end!='\0'))return false;
        next=end+1;
    }
    sample.time=values[0];sample.altitude=values[1];
    sample.gyro={values[2],values[3],values[4]};
    return sample.time>=0 && sample.time<=3600 &&
        sample.altitude>=-1000 && sample.altitude<=100000 &&
        std::fabs(sample.gyro.x)<=2000 && std::fabs(sample.gyro.y)<=2000 &&
        std::fabs(sample.gyro.z)<=2000;
}
} // namespace

bool TelemetryRecording::load(const char* path,std::string& error) {
    if(!path) {error="No recording path selected.";return false;}
    std::ifstream input(path,std::ios::binary);
    if(!input) {error="Cannot open recorded CSV at the selected path.";return false;}
    std::string line;
    if(!std::getline(input,line)) {error="Recording is empty.";return false;}
    if(!line.empty() && line.back()=='\r')line.pop_back();
    if(line!=columns) {error="CSV header must name seconds, metres and body gyro deg/s.";return false;}
    TelemetryRecording candidate;
    size_t bytes=line.size()+1;
    while(std::getline(input,line)) {
        bytes+=line.size()+1;
        if(bytes>1048576 || candidate.samples.size()>=20000) {
            error="Recording exceeds the 1 MiB / 20000-sample limit.";return false;
        }
        if(!line.empty() && line.back()=='\r')line.pop_back();
        TelemetrySample sample;
        if(line.size()>128 || !row(line,sample) ||
           (!candidate.samples.empty() && sample.time<=candidate.samples.back().time)) {
            error="Malformed CSV row, unit, value or non-increasing timestamp.";return false;
        }
        candidate.samples.push_back(sample);
    }
    if(input.bad() || candidate.samples.size()<2) {
        error="Recording needs at least two readable samples.";return false;
    }
    *this=std::move(candidate);
    error.clear();
    return true;
}

bool TelemetryRecording::calibrate(std::string& error) {
    calibrated=false;
    if(samples.empty()) {error="Load a recorded CSV first.";return false;}
    const double cutoff=samples.front().time+2.0;
    size_t count=0;
    Vec3 sum{};
    double altitudeSum=0,lo=samples.front().altitude,hi=lo;
    for(const TelemetrySample& s:samples) {
        if(s.time>cutoff)break;
        ++count;sum.x+=s.gyro.x;sum.y+=s.gyro.y;sum.z+=s.gyro.z;
        altitudeSum+=s.altitude;
        lo=std::min(lo,s.altitude);hi=std::max(hi,s.altitude);
    }
    if(count<10 || samples[count-1].time-samples.front().time<1.0) {
        error="Calibration needs >= 10 stationary readings over >= 1 second.";return false;
    }
    const Vec3 bias{sum.x/count,sum.y/count,sum.z/count};
    for(size_t i=0;i<count;++i) {
        const Vec3 g=samples[i].gyro;
        if(std::fabs(g.x-bias.x)>3 || std::fabs(g.y-bias.y)>3 ||
           std::fabs(g.z-bias.z)>3 || hi-lo>3) {
            error="Calibration rejected: the first two seconds are not stationary.";
            return false;
        }
    }
    for(size_t i=1;i<samples.size();++i) {
        if(samples[i].time-samples[i-1].time>0.25) {
            error="Gyro gaps above 0.25 s prevent relative attitude integration.";
            return false;
        }
    }
    gyroBias=bias;altitudeZero=altitudeSum/count;
    samples.front().attitude={};
    for(size_t i=1;i<samples.size();++i) {
        const TelemetrySample& a=samples[i-1];
        TelemetrySample& b=samples[i];
        const double dt=b.time-a.time;
        const Vec3 average{(a.gyro.x+b.gyro.x)*0.5-bias.x,
                           (a.gyro.y+b.gyro.y)*0.5-bias.y,
                           (a.gyro.z+b.gyro.z)*0.5-bias.z};
        const double magnitude=std::sqrt(average.x*average.x+average.y*average.y+
                                         average.z*average.z)*rad;
        const double half=magnitude*dt*0.5;
        const double scale=magnitude>1e-12?std::sin(half)/magnitude*rad:
            dt*0.5*rad;
        // Gyro rates are body-frame, so the incremental rotation multiplies
        // on the right. No accelerometer or magnetic correction is available.
        b.attitude=unit(product(a.attitude,{std::cos(half),
            average.x*scale,average.y*scale,average.z*scale}));
    }
    calibrated=true;error.clear();return true;
}

TelemetrySample TelemetryRecording::at(double time) const {
    if(samples.empty())return {};
    if(time<=samples.front().time)return samples.front();
    if(time>=samples.back().time)return samples.back();
    const auto it=std::lower_bound(samples.begin(),samples.end(),time,
        [](const TelemetrySample& s,double t){return s.time<t;});
    const auto& a=*(it-1);const auto& b=*it;
    const double f=(time-a.time)/(b.time-a.time);
    const auto lerp=[f](double x,double y){return x+(y-x)*f;};
    TelemetrySample result;
    result.time=time;result.altitude=lerp(a.altitude,b.altitude);
    result.gyro={lerp(a.gyro.x,b.gyro.x),lerp(a.gyro.y,b.gyro.y),
                 lerp(a.gyro.z,b.gyro.z)};
    Quaternion qb=b.attitude;
    if(a.attitude.w*qb.w+a.attitude.x*qb.x+a.attitude.y*qb.y+
       a.attitude.z*qb.z<0)qb={-qb.w,-qb.x,-qb.y,-qb.z};
    result.attitude=unit({lerp(a.attitude.w,qb.w),lerp(a.attitude.x,qb.x),
        lerp(a.attitude.y,qb.y),lerp(a.attitude.z,qb.z)});
    return result;
}
} // namespace eulo
