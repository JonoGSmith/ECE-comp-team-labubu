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

    // Prints a special message over serial when the button is pressed and released.
    inline void report_changes(){
        static bool gButtonPressed = false;
        auto active = poll();
        if(active && !gButtonPressed){
            console::println("Button 0: pressed");
        }else if(!active && gButtonPressed){
            console::println("Button 0: released");
        }
        gButtonPressed = active;
    }
}