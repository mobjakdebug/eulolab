#include "ps4_bench_usb.hpp"
#include "core/bench_usb_descriptors.hpp"
#include <cstdio>

namespace eulo {
namespace {
void usbError(std::string& error,const char* operation,int code) {
    char text[110];
    std::snprintf(text,sizeof(text),"USB %s failed (%d); check board and connection.",
                  operation,code);
    error=text;
}
}
bool OrbisBenchUsb::open(std::string& error) {
    close();
    if(!profile_.validTransport(error))return false;
    libusb_device** list=nullptr;
    const int count=sceUsbdGetDeviceList(&list);
    if(count<0) {
        if(list)sceUsbdFreeDeviceList(list);
        usbError(error,"enumeration",count);return false;
    }
    libusb_device* match=nullptr;
    int matches=0;
    for(int i=0;list && i<count;++i) {
        libusb_device_descriptor desc{};
        if(sceUsbdGetDeviceDescriptor(list[i],&desc)==0 &&
           desc.idVendor==profile_.vendor && desc.idProduct==profile_.product) {
            // Do not open a composite device or silently select one of two boards.
            if(desc.bNumConfigurations!=1 ||
               (desc.bDeviceClass!=0 && desc.bDeviceClass!=0xff &&
                desc.bDeviceClass!=0xef)) {
                error="Matching USB ID is not a single-config vendor board.";
                if(list)sceUsbdFreeDeviceList(list);
                return false;
            }
            match=list[i];++matches;
        }
    }
    if(matches==1) {
        const int rc=sceUsbdOpen(match,&device_);
        if(rc<0 || !device_) {
            usbError(error,"open",rc);
            device_=nullptr;
        }
    }
    if(list)sceUsbdFreeDeviceList(list);
    if(matches!=1) {
        error=matches?"More than one board with the configured USB ID; disconnect extras.":
                      "No USB board with the configured VID:PID found.";
        return false;
    }
    if(!device_)return false;
    uint8_t raw[512]={};
    const int got=sceUsbdGetDescriptor(device_,2,0,raw,sizeof(raw));
    bench::BulkEndpoints endpoints;
    int configuration=0;
    if(got<9 || got>static_cast<int>(sizeof(raw)) ||
       !bench::findBenchInterface(raw,static_cast<std::size_t>(got),endpoints,
                                  profile_.interfaceNumber,profile_.bulkOut,
                                  profile_.bulkIn) ||
       sceUsbdGetConfiguration(device_,&configuration)<0 ||
       configuration!=endpoints.configuration) {
        error="Bench USB configuration lacks the required single vendor interface and bulk IN/OUT.";
        close();return false;
    }
    const int rc=sceUsbdClaimInterface(device_,endpoints.interfaceNumber);
    if(rc<0) {
        usbError(error,"claim vendor interface",rc);
        close();return false;
    }
    claimed_=endpoints.interfaceNumber;
    in_=endpoints.in;out_=endpoints.out;
    error.clear();return true;
}
int OrbisBenchUsb::write(const uint8_t* bytes,std::size_t length,std::string& error) {
    if(!device_ || claimed_<0 || !bytes || length==0 || length>bench::maxFrame) {
        error="Bench USB is not open for bounded writes.";return -1;
    }
    int transferred=0;
    const int rc=sceUsbdBulkTransfer(device_,out_,
        const_cast<unsigned char*>(bytes),static_cast<int>(length),&transferred,120);
    if(rc<0 || transferred!=static_cast<int>(length)) {
        usbError(error,"bulk OUT",rc);return -1;
    }
    error.clear();return transferred;
}
int OrbisBenchUsb::read(uint8_t* bytes,std::size_t capacity,unsigned timeoutMs,
                        std::string& error) {
    if(!device_ || claimed_<0 || !bytes || capacity==0 || capacity>64 ||
       timeoutMs==0 || timeoutMs>100) {
        error="Bench USB is not open for bounded reads.";return -1;
    }
    int transferred=0;
    const int rc=sceUsbdBulkTransfer(device_,in_,bytes,
        static_cast<int>(capacity),&transferred,timeoutMs);
    // OpenOrbis Usbd exposes libusb-style -7 for a read timeout. A partial
    // transfer still carries bytes; other errors terminate the exchange.
    if(rc==-7 && transferred==0)return 0;
    if(rc<0 && rc!=-7) {
        usbError(error,"bulk IN",rc);return -1;
    }
    if(transferred<0 || transferred>static_cast<int>(capacity)) {
        error="Bench USB returned an invalid byte count.";return -1;
    }
    error.clear();return transferred;
}
void OrbisBenchUsb::close() {
    if(device_) {
        if(claimed_>=0)sceUsbdReleaseInterface(device_,claimed_);
        sceUsbdClose(device_);
    }
    device_=nullptr;claimed_=-1;in_=out_=0;
}
} // namespace eulo
