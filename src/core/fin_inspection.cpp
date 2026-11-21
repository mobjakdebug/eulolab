#include "fin_inspection.hpp"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>

namespace eulo {
namespace {
constexpr const char* header="fin_number,span_mm,root_chord_mm,tip_chord_mm,sweep_mm,thickness_mm,root_setback_mm,cant_deg,azimuth_deg";
bool next(std::ifstream& input,std::string& line) {
    if(!std::getline(input,line) || line.size()>128)return false;
    if(!line.empty() && line.back()=='\r')line.pop_back();
    for(unsigned char ch:line)
        if(ch<' ' || ch>'~')return false;
    return true;
}
bool label(const std::string& line,const char* key,std::string& result) {
    const std::string prefix=std::string(key)+"=";
    if(line.rfind(prefix,0)!=0 || line.size()<=prefix.size() ||
       line.size()-prefix.size()>64)return false;
    result=line.substr(prefix.size());
    if(result.front()==' ' || result.back()==' ')return false;
    for(unsigned char ch:result)if(ch<' ' || ch>'~' || ch=='=')return false;
    return true;
}
bool utc(const std::string& text) {
    if(text.size()!=20 || text[4]!='-' || text[7]!='-' || text[10]!='T' ||
       text[13]!=':' || text[16]!=':' || text[19]!='Z')return false;
    for(size_t i=0;i<19;++i)
        if(i!=4 && i!=7 && i!=10 && i!=13 && i!=16 &&
           (text[i]<'0' || text[i]>'9'))return false;
    const int year=std::atoi(text.substr(0,4).c_str());
    const int month=std::atoi(text.substr(5,2).c_str());
    const int day=std::atoi(text.substr(8,2).c_str());
    const int hour=std::atoi(text.substr(11,2).c_str());
    const int minute=std::atoi(text.substr(14,2).c_str());
    const int second=std::atoi(text.substr(17,2).c_str());
    constexpr int days[]={0,31,28,31,30,31,30,31,31,30,31,30,31};
    if(year<2000 || year>2199 || month<1 || month>12 || hour>23 ||
       minute>59 || second>59)return false;
    const int febLeap=(month==2 && year%4==0 && (year%100!=0 || year%400==0))?1:0;
    return day>=1 && day<=days[month]+febLeap;
}
bool reading(const std::string& line,int number,FinReading& result) {
    if(line.size()>128)return false;
    const char* p=line.c_str();
    if(*p<'0' || *p>'9')return false;
    char* end=nullptr;errno=0;
    const long parsed=std::strtol(p,&end,10);
    if(p==end || *end!=',' || errno==ERANGE || parsed!=number)return false;
    p=end+1;
    double values[8]={};
    for(int i=0;i<8;++i) {
        errno=0;values[i]=std::strtod(p,&end);
        // strtod accepts leading spaces; this CSV schema does not. A leading
        // plus sign is allowed, but empty/whitespace-only values are not.
        if(p==end || *p==' ' || *p=='\t' || errno==ERANGE ||
           !std::isfinite(values[i]) ||
           (i<7?*end!=',':*end!='\0'))return false;
        p=end+1;
    }
    // Parser bounds are input sanity checks, NOT aerodynamic/safety limits.
    if(values[0]<1 || values[0]>500 || values[1]<5 || values[1]>500 ||
       values[2]<0 || values[2]>500 || values[3]<-200 || values[3]>500 ||
       values[4]<0.1 || values[4]>50 || values[5]<0 || values[5]>3000 ||
       std::fabs(values[6])>30 || values[7]<0 || values[7]>=360)return false;
    result={number,values[0],values[1],values[2],values[3],values[4],
            values[5],values[6],values[7]};
    return true;
}
} // namespace

bool FinInspection::load(const char* path,std::string& error) {
    if(!path) {error="No fin measurement path.";return false;}
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) {error="Cannot open fin-inspection.csv beside the project.";return false;}
    const std::streamoff bytes=input.tellg();
    if(bytes<0 || bytes>8192) {error="Fin log must be a file <=8 KiB.";return false;}
    input.seekg(0);
    std::string line;
    if(!next(input,line) || line!="EULOFINS/1") {
        error="Fin log requires EULOFINS/1 header.";return false;
    }
    FinInspection candidate;
    if(!next(input,line) || !label(line,"observed_utc",candidate.observedUtc) ||
       !utc(candidate.observedUtc) ||
       !next(input,line) || !label(line,"operator",candidate.operatorName) ||
       !next(input,line) || !label(line,"instrument",candidate.instrument)) {
        error="Fin log needs UTC timestamp, operator and instrument (ASCII).";
        return false;
    }
    if(!next(input,line) || line.rfind("fin_count=",0)!=0 ||
       line.size()!=11 || line[10]<'2' || line[10]>'8') {
        error="Fin count must be an explicit integer from 2 to 8.";return false;
    }
    const int count=line[10]-'0';
    if(!next(input,line) || line!=header) {
        error="Fin log column names or units differ from EULOFINS/1.";return false;
    }
    for(int i=1;i<=count;++i) {
        FinReading value;
        if(!next(input,line) || !reading(line,i,value)) {
            error="Missing, out-of-order or invalid per-fin reading.";return false;
        }
        candidate.fins.push_back(value);
    }
    if(std::getline(input,line) || input.bad() ||
       (input.fail() && !input.eof())) {
        error="Unexpected extra data after the last measured fin.";return false;
    }
    *this=std::move(candidate);
    error.clear();return true;
}

bool FinInspection::valueForField(int field,size_t finIndex,double& value) const {
    if(finIndex>=fins.size())return false;
    const FinReading& fin=fins[finIndex];
    switch(field) {
        case 1:value=fin.spanMm;break;
        case 2:value=fin.rootMm;break;
        case 3:value=fin.tipMm;break;
        case 4:value=fin.sweepMm;break;
        case 5:value=fin.thicknessMm;break;
        case 6:value=fin.setbackMm;break;
        default:return false;
    }
    return true;
}
bool FinInspection::rangeForField(int field,double& low,double& high) const {
    if(!valueForField(field,0,low))return false;
    high=low;
    for(size_t i=1;i<fins.size();++i) {
        double value=0;
        if(!valueForField(field,i,value))return false;
        low=std::min(low,value);
        high=std::max(high,value);
    }
    return true;
}

double FinInspection::maxAbsCant() const {
    double maximum=0;
    for(const auto& fin:fins)maximum=std::max(maximum,std::fabs(fin.cantDeg));
    return maximum;
}
} // namespace eulo
