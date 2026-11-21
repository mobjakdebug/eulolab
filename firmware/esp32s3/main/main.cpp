// ESP-IDF ESP32-S3 reference target for EuloBench/1. NOT flight firmware.
#include "core/bench_board.hpp"
#include "core/bench_wire.hpp"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_task_wdt.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include <cstdint>

#ifndef CONFIG_IDF_TARGET_ESP32S3
#error "This USB servo reference is only for ESP32-S3 native USB OTG."
#endif
#if CONFIG_EULO_BENCH_USB_VID == 0 || CONFIG_EULO_BENCH_USB_PID == 0
#error "Set the board's authorized VID and PID in idf.py menuconfig before building."
#endif
#if CONFIG_EULO_BENCH_GATE_PIN < 0 || CONFIG_EULO_BENCH_SERVO0_PIN < 0 || \
    CONFIG_EULO_BENCH_SERVO1_PIN < 0 || CONFIG_EULO_BENCH_SERVO2_PIN < 0 || \
    CONFIG_EULO_BENCH_SERVO3_PIN < 0
#error "Choose the actual board's gate and four PWM GPIOs in idf.py menuconfig."
#endif
#if CONFIG_TINYUSB_VENDOR_COUNT != 1
#error "EuloBench requires exactly one vendor interface."
#endif
namespace {
constexpr int pins[]={CONFIG_EULO_BENCH_SERVO0_PIN,CONFIG_EULO_BENCH_SERVO1_PIN,
                      CONFIG_EULO_BENCH_SERVO2_PIN,CONFIG_EULO_BENCH_SERVO3_PIN};
constexpr int gatePin=CONFIG_EULO_BENCH_GATE_PIN;
static_assert(gatePin!=19 && gatePin!=20,"Gate cannot share the ESP32-S3 USB pins");
static_assert(gatePin!=pins[0] && gatePin!=pins[1] && gatePin!=pins[2] &&
              gatePin!=pins[3],"Gate GPIO cannot drive a servo");
static_assert(pins[0]!=pins[1] && pins[0]!=pins[2] && pins[0]!=pins[3] &&
              pins[1]!=pins[2] && pins[1]!=pins[3] && pins[2]!=pins[3],
              "Servo GPIOs must be distinct");
static_assert(pins[0]!=19 && pins[0]!=20 && pins[1]!=19 && pins[1]!=20 &&
              pins[2]!=19 && pins[2]!=20 && pins[3]!=19 && pins[3]!=20,
              "PWM GPIOs cannot share the native USB pins");
constexpr uint8_t configBytes[]={
    TUD_CONFIG_DESCRIPTOR(1,1,0,32,0,250),
    9,TUSB_DESC_INTERFACE,0,0,2,TUSB_CLASS_VENDOR_SPECIFIC,0x45,0x55,0,
    7,TUSB_DESC_ENDPOINT,0x01,TUSB_XFER_BULK,64,0,0,
    7,TUSB_DESC_ENDPOINT,0x81,TUSB_XFER_BULK,64,0,0
};
static_assert(sizeof(configBytes)==32,"USB configuration length mismatch");
const tusb_desc_device_t deviceDesc={
    sizeof(tusb_desc_device_t),TUSB_DESC_DEVICE,0x0200,
    TUSB_CLASS_VENDOR_SPECIFIC,0x45,0x55,CFG_TUD_ENDPOINT0_SIZE,
    CONFIG_EULO_BENCH_USB_VID,CONFIG_EULO_BENCH_USB_PID,0x0100,
    1,2,0,1
};
char language[]={0x09,0x04,0};
const char* strings[]={language,"EuloLab bench reference","EuloBench/1"};
portMUX_TYPE boardMux=portMUX_INITIALIZER_UNLOCKED;
eulo::bench::Board board;
eulo::bench::Decoder decoder;
bool usbPresent=false;
uint32_t milliseconds() {return static_cast<uint32_t>(esp_timer_get_time()/1000);}
bool gateClosed() {return gpio_get_level(static_cast<gpio_num_t>(gatePin))==1;}
void usbEvent(tinyusb_event_t* event,void*) {
    const bool present=event->id==TINYUSB_EVENT_ATTACHED ||
                       event->id==TINYUSB_EVENT_RESUMED;
    if(event->id!=TINYUSB_EVENT_ATTACHED && event->id!=TINYUSB_EVENT_RESUMED &&
       event->id!=TINYUSB_EVENT_DETACHED && event->id!=TINYUSB_EVENT_SUSPENDED)return;
    const uint32_t now=milliseconds();
    portENTER_CRITICAL(&boardMux);
    usbPresent=present;
    board.tick(now,present,gateClosed());
    portEXIT_CRITICAL(&boardMux);
    if(!present)decoder.reset();
}
uint32_t duty(int angleCdeg) {
    // Illustrative 90 deg / 1 ms servo scale; no position feedback.
    const int micros=1500+angleCdeg*1000/9000;
    return static_cast<uint32_t>(micros*16384/20000);
}
void setServo(int axis,int angleCdeg) {
    const auto channel=static_cast<ledc_channel_t>(LEDC_CHANNEL_0+axis);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE,channel,duty(angleCdeg)));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE,channel));
}
void initOutputs() {
    gpio_config_t gate{};
    gate.pin_bit_mask=1ULL<<gatePin;
    gate.mode=GPIO_MODE_INPUT;
    gate.pull_up_en=GPIO_PULLUP_DISABLE;
    gate.pull_down_en=GPIO_PULLDOWN_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gate));
    ledc_timer_config_t timer{};
    timer.speed_mode=LEDC_LOW_SPEED_MODE;
    timer.duty_resolution=LEDC_TIMER_14_BIT;
    timer.timer_num=LEDC_TIMER_0;
    timer.freq_hz=50;
    timer.clk_cfg=LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    for(int i=0;i<4;++i) {
        ledc_channel_config_t channel{};
        channel.gpio_num=pins[i];
        channel.speed_mode=LEDC_LOW_SPEED_MODE;
        channel.channel=static_cast<ledc_channel_t>(LEDC_CHANNEL_0+i);
        channel.timer_sel=LEDC_TIMER_0;
        channel.duty=duty(0);
        channel.hpoint=0;
        ESP_ERROR_CHECK(ledc_channel_config(&channel));
    }
}
} // namespace

// Vendor callbacks run in TinyUSB's task. The independent main task applies
// PWM and neutralises on deadline, gate loss, detach or USB suspend.
extern "C" {
#if CFG_TUD_API_V0_19_COMPAT
void tud_vendor_rx_cb(uint8_t itf,const uint8_t*,uint16_t)
#else
void tud_vendor_rx_cb(uint8_t itf,const uint8_t*,uint32_t)
#endif
{
    if(itf!=0)return;
    uint8_t input[64];
    while(tud_vendor_available()) {
        const uint32_t count=tud_vendor_read(input,sizeof(input));
        if(!count)break;
        for(uint32_t i=0;i<count;++i) {
            eulo::bench::Frame request,response;
            if(!decoder.push(input[i],request))continue;
            const uint32_t now=milliseconds();
            portENTER_CRITICAL(&boardMux);
            usbPresent=true;
            const bool reply=board.receive(request,now,true,gateClosed(),response);
            portEXIT_CRITICAL(&boardMux);
            if(reply) {
                uint8_t packet[eulo::bench::maxFrame];
                const auto length=eulo::bench::encode(response,packet);
                if(length && tud_vendor_write(packet,length)==length)
                    tud_vendor_write_flush();
            }
        }
    }
}
void app_main() {
    initOutputs();
    ESP_ERROR_CHECK(esp_task_wdt_add(nullptr));
    tinyusb_config_t usb=TINYUSB_DEFAULT_CONFIG(usbEvent);
    usb.descriptor.device=&deviceDesc;
    usb.descriptor.string=strings;
    usb.descriptor.string_count=sizeof(strings)/sizeof(strings[0]);
    usb.descriptor.full_speed_config=configBytes;
    ESP_ERROR_CHECK(tinyusb_driver_install(&usb));
    int applied[4]={};
    for(;;) {
        const uint32_t now=milliseconds();
        int requested[4];
        portENTER_CRITICAL(&boardMux);
        board.tick(now,usbPresent,gateClosed());
        for(int i=0;i<4;++i)requested[i]=board.angle(i);
        portEXIT_CRITICAL(&boardMux);
        for(int i=0;i<4;++i)if(requested[i]!=applied[i]) {
            setServo(i,requested[i]);applied[i]=requested[i];
        }
        ESP_ERROR_CHECK(esp_task_wdt_reset());
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
} // extern "C"
