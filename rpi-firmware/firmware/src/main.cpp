#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "tusb.h"

#include "dev/mic_adc.hpp"
#include "dev/servo.hpp"
#include "dev/usbuart.hpp"
#include "dev/i2s_dac.hpp"

void init(){
    dev::usb::init();
    dev::mic::init();
    dev::servo::init();
    dev::dac::init();

    if(cyw43_arch_init()){ // Initialise the Wi-Fi chip
        printf("Wi-Fi init failed\n");
    }

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1); // Turn on the Pico W LED as proof of life.
}

int main(){
    init();

    while(true){
        printf("Hello, world!\n");
        dev::servo::set_rotation_angle(-90);
        sleep_ms(3000);
        dev::servo::set_rotation_angle(0);
        sleep_ms(3000);
        dev::servo::set_rotation_angle(90);
        sleep_ms(3000);
    }
}
