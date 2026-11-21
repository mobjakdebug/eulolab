#include "bench_profile.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace eulo::bench {
namespace {
constexpr const char* keys[]={"family","vendor","product","interface","bulk_out",
                              "bulk_in","pitch","yaw","fin_a","fin_b"};
bool validEndpoint(uint8_t address,bool input) {
    return address==0 || ((address&0x70)==0 && (address&0x0f)!=0 &&
                          ((address&0x80)!=0)==input);
}
unsigned field(const Profile& p,int i) {
    switch(i) {
        case 0:return static_cast<unsigned>(p.family);
        case 1:return p.vendor;
        case 2:return p.product;
        case 3:return p.interfaceNumber;
        case 4:return p.bulkOut;
        case 5:return p.bulkIn;
        default:return p.pins[static_cast<std::size_t>(i-6)];
    }
}
bool assign(Profile& p,int i,unsigned value) {
    if(value>(i==1 || i==2?0xffffu:0xffu))return false;
    switch(i) {
        case 0:p.family=static_cast<Family>(value);break;
        case 1:p.vendor=static_cast<uint16_t>(value);break;
        case 2:p.product=static_cast<uint16_t>(value);break;
        case 3:p.interfaceNumber=static_cast<uint8_t>(value);break;
        case 4:p.bulkOut=static_cast<uint8_t>(value);break;
        case 5:p.bulkIn=static_cast<uint8_t>(value);break;
        default:p.pins[static_cast<std::size_t>(i-6)]=static_cast<uint8_t>(value);
    }
    return true;
}
}
bool Profile::validTransport(std::string& error) const {
    if(static_cast<unsigned>(family)>static_cast<unsigned>(Family::Other)) {
        error="Select a supported board-family label.";return false;
    }
    if(!vendor || !product) {
        error="Set your board's authorized nonzero VID and PID in Bench setup.";return false;
    }
    if(!validEndpoint(bulkOut,false) || !validEndpoint(bulkIn,true)) {
        error="Bulk OUT/IN must have the right direction and endpoint 1..15, or Auto 00.";
        return false;
    }
    error.clear();return true;
}
bool Profile::validDraft(std::string& error) const {
    if(!validTransport(error))return false;
    for(std::size_t i=0;i<pins.size();++i) {
        const uint8_t pin=pins[i];
        if(pin==0xff)continue;
        if((family==Family::Esp32 && pin>48) ||
           (family==Family::Stm32 && pin>0x7f)) {
            error="Pin code exceeds the chosen board-family range.";return false;
        }
        for(std::size_t j=0;j<i;++j)if(pin==pins[j]) {
            error="Each servo axis needs a distinct board pin.";return false;
        }
    }
    error.clear();return true;
}
bool Profile::valid(std::string& error) const {
    if(!validDraft(error))return false;
    for(uint8_t pin:pins)if(pin==0xff) {
        error="Set four board-approved pins; FF means unset.";return false;
    }
    error.clear();return true;
}
const char* familyName(Family family) {
    switch(family) {
        case Family::Any:return "Protocol device";
        case Family::Esp32:return "ESP32 GPIO";
        case Family::Stm32:return "STM32 port/pin";
        case Family::Other:return "Other firmware";
    }
    return "Unknown";
}
std::string pinName(Family family,uint8_t pin) {
    if(pin==0xff)return "unset";
    if(family==Family::Stm32 && pin<=0x7f)
        return std::string("P")+static_cast<char>('A'+(pin>>4))+
               std::to_string(pin&15);
    if(family==Family::Esp32)return "GPIO"+std::to_string(pin);
    char hex[5];std::snprintf(hex,sizeof(hex),"%02X",pin);
    return std::string("pin ")+hex;
}
bool loadProfile(Profile& profile,const char* path,std::string& error) {
    if(!path || !*path) {error="No bench profile path.";return false;}
    FILE* file=std::fopen(path,"rb");
    if(!file) {error="No saved bench profile; USB is disabled.";return false;}
    char line[128];
    Profile parsed;
    bool ok=std::fgets(line,sizeof(line),file) &&
            std::strcmp(line,"EULOBENCH2\n")==0;
    for(int i=0;ok && i<10;++i) {
        if(!std::fgets(line,sizeof(line),file)) {ok=false;break;}
        const std::size_t length=std::strlen(keys[i]);
        if(std::strncmp(line,keys[i],length)!=0 || line[length]!='=') {
            ok=false;break;
        }
        char* end=nullptr;
        const unsigned long value=std::strtoul(line+length+1,&end,16);
        if(end==line+length+1 || (*end!='\n' && *end!='\0') ||
           !assign(parsed,i,static_cast<unsigned>(value)) || value>0xffffu) {
            ok=false;break;
        }
    }
    if(ok && std::fgetc(file)!=EOF)ok=false;
    if(std::ferror(file))ok=false;
    if(std::fclose(file)!=0)ok=false;
    if(!ok || !parsed.validDraft(error)) {
        if(ok)return false;
        error="Invalid EULOBENCH2 profile; no USB device will be opened.";
        return false;
    }
    profile=parsed;error.clear();return true;
}
bool saveProfile(const Profile& profile,const char* path,std::string& error) {
    if(!profile.validDraft(error))return false;
    if(!path || !*path) {error="No bench profile path.";return false;}
    const std::string temporary=std::string(path)+".tmp";
    FILE* file=std::fopen(temporary.c_str(),"wb");
    if(!file) {error="Cannot create bench profile in this storage directory.";return false;}
    bool ok=std::fputs("EULOBENCH2\n",file)>=0;
    for(int i=0;ok && i<10;++i)ok=std::fprintf(file,"%s=%X\n",keys[i],field(profile,i))>0;
    if(std::fflush(file)!=0)ok=false;
    if(std::fclose(file)!=0)ok=false;
    if(ok)ok=std::rename(temporary.c_str(),path)==0;
    if(!ok) {
        std::remove(temporary.c_str());
        error="Cannot save bench profile atomically; check storage.";
        return false;
    }
    error.clear();return true;
}
} // namespace eulo::bench
