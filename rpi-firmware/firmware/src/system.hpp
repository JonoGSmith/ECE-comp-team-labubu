#pragma once
#include "common.hpp"

namespace sys{
    constexpr u32 cClockRate = 144'000'000; // 144 MHz is a multiple of 48k. Default speed is 125MHz
}
using DMAChannel = u8; // ID
