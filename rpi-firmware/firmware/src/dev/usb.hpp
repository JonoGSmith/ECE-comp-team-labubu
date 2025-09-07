#pragma once
#include "../common.hpp"
#include "pico/stdlib.h"
#include "tusb.h"

namespace dev::usb{
    inline void init(){
        board_init();
        tusb_init();
        stdio_init_all();
    }
    inline void tick(){
        tud_task();
    }
}