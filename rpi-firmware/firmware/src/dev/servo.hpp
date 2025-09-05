#pragma once
#include "../common.hpp"
#include <hardware/pwm.h>

// For driving the servo that rotates the head/body of the doll.
// SG90 9 g Micro Servo.
// All I control is a single GPIO PWM to determine the direction of rotation.
// ------------------------------

namespace dev::servo{
    namespace cfg{
        constexpr u32 GPIO_PIN = 2;
        constexpr u32 FREQUENCY = 50;

        // This should give 0.1ms precision (10k divisions)
        constexpr f32 PWM_CLOCK_DIV = 250;
        constexpr f64 PWM_CLOCK_RATE = sys::cClockRate / PWM_CLOCK_DIV;
        constexpr f64 PWM_DIVISIONS = PWM_CLOCK_RATE / FREQUENCY;
        constexpr u16 PWM_COUNT_TOP = PWM_DIVISIONS - 1;
        static_assert(PWM_DIVISIONS <= UINT16_MAX, "The selected divider cannot create the PWM signal");
    }

    // Global variables
    // -----------------------
    u8 gPWMSlice;

    // sad forward declarations
    constexpr void set_rotation_angle(f32 degrees);

    // Functions
    // -----------------------

    inline void init(){
        using namespace cfg;

        gpio_set_function(GPIO_PIN, GPIO_FUNC_PWM);
        gPWMSlice = pwm_gpio_to_slice_num(GPIO_PIN);
        pwm_set_clkdiv(gPWMSlice, PWM_CLOCK_DIV);
        pwm_set_wrap(gPWMSlice, PWM_COUNT_TOP);
        set_rotation_angle(0);
        pwm_set_enabled(gPWMSlice, true);
    }

    // -90 to 90 degrees. We don't appear to be able to control speed.
    // NOTE: Currently this seems to only do half the angle.
    constexpr void set_rotation_angle(f32 degrees){
        using namespace cfg;
        f32 safe_degrees = clamp(-90.0f, degrees, 90.0f); // safety
        // -90deg corresponds to a 1ms PWM high period (datasheet)
        //   0deg: 1.5ms
        // +90deg: 2ms
        f32 period = 1.5 + (safe_degrees / 180); // in milliseconds
        // pwm period: (period/1000) / (1/FREQUENCY) * PWM_DIVISIONS
        //             period * (FREQUENCY/1000*PWM_DIVISIONS)
        constexpr f32 multiplier = FREQUENCY * PWM_DIVISIONS / 1000.f;

        // Write to the PWM unit.
        u16 count = period * multiplier;
        pwm_set_chan_level(gPWMSlice, PWM_CHAN_A, count);
    }
}