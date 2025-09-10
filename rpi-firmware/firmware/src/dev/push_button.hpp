#include "../common.hpp"
#include "../console.hpp"
#include "pico/stdlib.h"

// Simple GPIO button
// Active low, high = 3.3V (pull up)
namespace dev::btn{
    constexpr bool cActiveEdge = false; // Active low
    constexpr u8 cGPIOPin = 3;

    inline void init(){
        gpio_init(cGPIOPin);
        gpio_set_dir(cGPIOPin, false); // false = input
        gpio_pull_up(cGPIOPin);        // enable internal pull-up
    }
    // Is the button currently pressed?
    inline bool poll(){
        return gpio_get(cGPIOPin) == cActiveEdge;
    }

    // Most overengineered denoising function ever.
    // The denoised value is schmitt-triggered, provided the value stays the same over 3ms.
    // Additionally, the denoised value has a minimum pulse width of 50ms before switching states.
    inline bool poll_denoised(){
        static absolute_time_t next_poll_time = 0;
        static array<bool, 3> history = {false, false, false};
        static u32 i = 0;
        static bool current = false;
        static absolute_time_t cooldown = 0;
        constexpr u32 MAX_COOLDOWN_MS = 50;

        auto now = get_absolute_time();
        if(absolute_time_diff_us(now, next_poll_time) <= 0){
            next_poll_time = make_timeout_time_ms(1);
            history[i] = poll();
            i = (i+1) % history.size();
            if(absolute_time_diff_us(now, cooldown) <= 0){
                if(current == false && std::ranges::all_of(history, cmpeqfn(true))){
                    current = true;
                    cooldown = make_timeout_time_ms(MAX_COOLDOWN_MS);
                }else if(current == true && std::ranges::all_of(history, cmpeqfn(false))){
                    current = false;
                    cooldown = make_timeout_time_ms(MAX_COOLDOWN_MS);
                }
            }
        }
        return current;
    }

    // Prints a special message over serial when the button is pressed and released.
    inline void report_changes(){
        static bool gButtonPressed = false;
        auto active = poll_denoised();
        if(active && !gButtonPressed){
            console::println("Button 0: pressed");
        }else if(!active && gButtonPressed){
            console::println("Button 0: released");
        }
        gButtonPressed = active;
    }
}