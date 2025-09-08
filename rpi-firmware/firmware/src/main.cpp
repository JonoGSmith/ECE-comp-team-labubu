#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/clocks.h"
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
    set_sys_clock_khz(sys::cClockRate / 1000, true);

    dev::usb::init();
    while(to_ms_since_boot(get_absolute_time()) < 3000){ // Wait to connect device to PC - debugging.
        dev::usb::tick();
    }

    dev::servo::init();
    dev::mic::init();
    dev::dac::init();

    if(cyw43_arch_init()){ // Initialise the Wi-Fi chip
        printf("Wi-Fi init failed\n");
    }
    set_obled(true); // Turn on the Pico W LED as proof of life.
}

decltype(dev::dac::gAudioRecvBuffer.ring) copy;

int main(){
    bool light_toggle = true;
    init();

    // printf("Hello, world! Playing %d samples.\n", gTestAudioSize / sizeof(u16));
    printf("WARNING! Use the headphone jack at your own risk. It can destroy your ears!\n");
    dev::dac::start();
    dev::mic::start();

    auto once_per_second = make_timeout_time_ms(1000); // not strictly, but its ok.
    auto cook = make_timeout_time_ms(5000); // not strictly, but its ok.
    bool done = false;
    while(true){
        auto now = get_absolute_time();
        dev::usb::tick();

        // Sleeping is now illegal.
        if(absolute_time_diff_us(now, once_per_second) <= 0){
            set_obled(light_toggle);
            light_toggle = !light_toggle;

            printf("DMAcnt: spk %d, mic %d\n", dev::dac::isDMA, dev::mic::gDMACount);
            dev::dac::isDMA = 0;
            dev::mic::gDMACount = 0;
            once_per_second = delayed_by_ms(now, 1000);
        }

        // if(!done && absolute_time_diff_us(now, cook) <= 0){
        //     done = true;
        //     copy = dev::dac::gAudioRecvBuffer.ring;
        //     size_t i = 0;
        //     for(auto const& e: copy){
        //         if(i % 16 == 0){
        //             printf("\n");
        //         }
        //         u16 raw = e;
        //         printf("%04x ", (unsigned int)raw);
        //         i++;
        //         if(i % 512 == 0){
        //             dev::usb::tick();
        //         }
        //     }
        //     printf("\n");
        // }
        // dev::servo::set_rotation_angle(-90);
    }
}
