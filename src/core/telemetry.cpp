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
constexpr const char* legacyColumns="time_s,altitude_m,gx_deg_s,gy_deg_s,gz_deg_s";
constexpr const char* sensorColumns="time_s,pressure_pa,temperature_c,gx_deg_s,gy_deg_s,gz_deg_s,ax_m_s2,ay_m_s2,az_m_s2";
constexpr double maxGyroGapS=.25;
constexpr std::streamoff maxBytes=2097152; // also enforced by the browser importer

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
double barometricHeight(double pressure,double reference) {
    // Standard-atmosphere approximation over a short vertical range. This is
    // only a difference relative to the observed stationary pressure.
    return 44330.0*(1-std::pow(pressure/reference,0.190294957));
}
bool row(const std::string& line,RecordingFormat format,TelemetrySample& sample) {
    const char* next=line.c_str();
    double values[9]={};
    const int count=format==RecordingFormat::BarometerImu?9:5;
    for(int i=0;i<count;++i) {
        char* end=nullptr;errno=0;
        values[i]=std::strtod(next,&end);
        if(next==end || !std::isfinite(values[i]) || errno==ERANGE ||
           (i<count-1?*end!=',':*end!='\0'))return false;
        next=end+1;
    }
    sample.time=values[0];
    if(format==RecordingFormat::BarometerImu) {
        sample.pressurePa=values[1];sample.temperatureC=values[2];
        sample.gyro={values[3],values[4],values[5]};
        sample.acceleration={values[6],values[7],values[8]};
        if(sample.pressurePa<30000 || sample.pressurePa>125000 ||
           sample.temperatureC < -40 || sample.temperatureC > 85 ||
           std::fabs(sample.acceleration.x)>200 ||
           std::fabs(sample.acceleration.y)>200 ||
           std::fabs(sample.acceleration.z)>200)return false;
    } else {
        sample.altitude=values[1];sample.gyro={values[2],values[3],values[4]};
        if(sample.altitude < -1000 || sample.altitude > 100000)return false;
    }
    return sample.time>=0 && sample.time<=3600 &&
        std::fabs(sample.gyro.x)<=2000 && std::fabs(sample.gyro.y)<=2000 &&
        std::fabs(sample.gyro.z)<=2000;
}
} // namespace

bool TelemetryRecording::load(const char* path,std::string& error) {
    if(!path) {error="No recording path selected.";return false;}
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) {error="Cannot open recorded CSV at the selected path.";return false;}
    const std::streamoff bytes=input.tellg();
    if(bytes<0 || bytes>maxBytes) {
        error="Recording exceeds the 2 MiB file limit or is not seekable.";return false;
    }
    input.seekg(0);
    std::string line;
    if(!std::getline(input,line)) {error="Recording is empty.";return false;}
    if(!line.empty() && line.back()=='\r')line.pop_back();
    RecordingFormat chosen;
    if(line==sensorColumns)chosen=RecordingFormat::BarometerImu;
    else if(line==legacyColumns)chosen=RecordingFormat::LegacyAltitude;
    else {error="Unknown CSV header; use the exact named units for legacy or barometer/IMU.";return false;}
    TelemetryRecording candidate;
    candidate.format=chosen;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r')line.pop_back();
        TelemetrySample sample;
        if(line.size()>192 || candidate.samples.size()>=20000 ||
           !row(line,chosen,sample) ||
           (!candidate.samples.empty() && sample.time<=candidate.samples.back().time)) {
            error="Malformed CSV row, unit, value, sample count or non-increasing timestamp.";
            return false;
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
    for(auto& s:samples)s.attitudeValid=false;
    if(samples.empty()) {error="Load a recorded CSV first.";return false;}
    const double cutoff=samples.front().time+2.0;
    size_t count=0;
    Vec3 sum{};
    double altitudeSum=0,pressureSum=0;
    double lo=samples.front().altitude,hi=lo;
    for(const TelemetrySample& s:samples) {
        if(s.time>cutoff)break;
        ++count;sum.x+=s.gyro.x;sum.y+=s.gyro.y;sum.z+=s.gyro.z;
        altitudeSum+=s.altitude;pressureSum+=s.pressurePa;
        lo=std::min(lo,s.altitude);hi=std::max(hi,s.altitude);
    }
    if(count<10 || samples[count-1].time-samples.front().time<1.0) {
        error="Calibration needs >= 10 stationary readings over >= 1 second.";return false;
    }
    const Vec3 bias{sum.x/count,sum.y/count,sum.z/count};
    if(std::fabs(bias.x)>3 || std::fabs(bias.y)>3 || std::fabs(bias.z)>3) {
        error="Calibration rejected: gyro mean is too large to treat as bias.";return false;
    }
    const double pressureMean=pressureSum/count;
    for(size_t i=0;i<count;++i) {
        const TelemetrySample& s=samples[i];
        if(i && s.time-samples[i-1].time>maxGyroGapS) {
            error="Calibration window has a missing-sample gap.";return false;
        }
        if(std::fabs(s.gyro.x-bias.x)>3 || std::fabs(s.gyro.y-bias.y)>3 ||
           std::fabs(s.gyro.z-bias.z)>3) {
            error="Calibration rejected: gyro moved in the first two seconds.";return false;
        }
        if(hasPressure()) {
            const Vec3 a=s.acceleration;
            const double norm=std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z);
            if(norm<7.0 || norm>12.5 ||
               std::fabs(barometricHeight(s.pressurePa,pressureMean))>3.0) {
                error="Calibration rejected: acceleration/pressure is not stationary.";
                return false;
            }
        }
    }
    if(!hasPressure() && hi-lo>3.0) {
        error="Calibration rejected: altitude changed in the first two seconds.";
        return false;
    }
    gyroBias=bias;
    if(hasPressure()) {
        pressureZeroPa=pressureMean;
        altitudeZero=0;
        for(auto& s:samples)s.altitude=barometricHeight(s.pressurePa,pressureZeroPa);
    } else {
        altitudeZero=altitudeSum/count;
    }
    samples.front().attitude={};
    samples.front().attitudeValid=true;
    for(size_t i=1;i<samples.size();++i) {
        const TelemetrySample& a=samples[i-1];
        TelemetrySample& b=samples[i];
        const double dt=b.time-a.time;
        if(!a.attitudeValid || dt>maxGyroGapS) {
            // There is no pose anchor after a missing interval. Never interpolate
            // or resume a gyro-integrated orientation on the far side of a gap.
            b.attitudeValid=false;
            continue;
        }
        const Vec3 average{(a.gyro.x+b.gyro.x)*0.5-bias.x,
                           (a.gyro.y+b.gyro.y)*0.5-bias.y,
                           (a.gyro.z+b.gyro.z)*0.5-bias.z};
        const double magnitude=std::sqrt(average.x*average.x+average.y*average.y+
                                         average.z*average.z)*rad;
        const double half=magnitude*dt*0.5;
        const double scale=magnitude>1e-12?std::sin(half)/magnitude*rad:
            dt*0.5*rad;
        // Gyro rates are body-frame, so the incremental rotation multiplies
        // on the right. There is no accelerometer/magnetic drift correction.
        b.attitude=unit(product(a.attitude,{std::cos(half),
            average.x*scale,average.y*scale,average.z*scale}));
        b.attitudeValid=true;
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
    const double dt=b.time-a.time;
    if(dt>maxGyroGapS) {
        // Show the nearest actual reading, not a fabricated interpolant. There
        // is no orientation at a cursor position between the two readings.
        TelemetrySample nearest=time-a.time<=b.time-time?a:b;
        nearest.attitudeValid=false;
        return nearest;
    }
    const double f=(time-a.time)/dt;
    const auto lerp=[f](double x,double y){return x+(y-x)*f;};
    TelemetrySample result;
    result.time=time;result.altitude=lerp(a.altitude,b.altitude);
    result.pressurePa=lerp(a.pressurePa,b.pressurePa);
    result.temperatureC=lerp(a.temperatureC,b.temperatureC);
    result.gyro={lerp(a.gyro.x,b.gyro.x),lerp(a.gyro.y,b.gyro.y),
                 lerp(a.gyro.z,b.gyro.z)};
    result.acceleration={lerp(a.acceleration.x,b.acceleration.x),
                         lerp(a.acceleration.y,b.acceleration.y),
                         lerp(a.acceleration.z,b.acceleration.z)};
    result.attitudeValid=a.attitudeValid && b.attitudeValid;
    if(result.attitudeValid) {
        Quaternion qb=b.attitude;
        if(a.attitude.w*qb.w+a.attitude.x*qb.x+a.attitude.y*qb.y+
           a.attitude.z*qb.z<0)qb={-qb.w,-qb.x,-qb.y,-qb.z};
        result.attitude=unit({lerp(a.attitude.w,qb.w),lerp(a.attitude.x,qb.x),
            lerp(a.attitude.y,qb.y),lerp(a.attitude.z,qb.z)});
    }
    return result;
}
} // namespace eulo
