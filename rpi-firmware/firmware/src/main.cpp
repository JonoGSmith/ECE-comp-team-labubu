#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "dev/mic_adc.hpp"
#include "dev/servo.hpp"
#include "dev/usb.hpp"
#include "dev/i2s_dac.hpp"

void set_obled(bool on){
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
}

void init(){
    dev::usb::init();
    sleep_ms(1000); // Let the USB Host discover us...

    dev::servo::init();
    // dev::mic::init();
    dev::dac::init();

    if(cyw43_arch_init()){ // Initialise the Wi-Fi chip
        printf("Wi-Fi init failed\n");
    }
    set_obled(true); // Turn on the Pico W LED as proof of life.
}

int main(){
    bool light_toggle = true;
    init();

    printf("Hello, world! Playing %d samples.\n", gTestAudioSize / sizeof(u16));
    dev::dac::start();
    while(true){
        dev::usb::tick();
        // Sleeping is now illegal.

//         set_obled(light_toggle);
//         light_toggle = !light_toggle;
//
//         if(dev::dac::isDMA){
//             printf("DMA now %d\n", dev::dac::isDMA);
//             dev::dac::isDMA = 0;
//         }else{
//             printf("no dma...?\n");
//         }
        // dev::servo::set_rotation_angle(-90);
    }
}
