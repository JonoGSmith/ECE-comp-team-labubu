#pragma once
#include "../common.hpp"
#include "pico/stdlib.h"

namespace dev::usb{
    inline void init(){
        stdio_init_all();
    }
}