#pragma once
#include "common.hpp"
#include "system.hpp"
#include <charconv>
#include <system_error>
#include "dev/servo_pwm.hpp"

namespace console{
    inline bool gPrintDebugInfo = false;

    template<size_t N>
    constexpr auto print(StringLitC<N> fmt, auto ref... params){
        auto ptr = &fmt[0];
        printf(fmt, params...);
    }

    template<size_t N> constexpr auto println(StringLitC<N> fmt, auto ref... params){
        printf(fmt, params...);
        printf("\n");
    }

    constexpr auto dbg(auto ref... params){
        if(gPrintDebugInfo){
            print("DBG: ");
            print(params...);
        }
    }
    constexpr auto dbgln(auto ref... params){
        if(gPrintDebugInfo){
            dbg(params...);
            printf("\n");
        }
    }

    // Process a console command.
    inline void processline(sv str){
        constexpr sv cmdServo = "servo";
        if(str.starts_with("help")){
            println(
R"(>>> ECE Competition Control Panel ver.R1
Credits:
- Leon, Michelle: Hardware leads.
- Jonathan Goldsmith: Firmware all.
- Oshan, Dave, Ibrahim: Software developers.
- Dave: Marketing material

Commands (submit a command by sending a '\n' newline):
    help            : Displays this
    debug <off/on>  : Controls printing debug info to the console
    servo <angle>   : Adjust the servo angle. `angle: decimal` ranged -90..=90
                      E.g.: `servo -15.2`
    areyouthepico?  : Replies `yes`
Messages the device will send:
    "Button 0: pressed" (or released)
    "DBG: debug message log"
)");
        }else if(str.starts_with(cmdServo)){
            f32 r;
            std::from_chars_result res = std::from_chars(str.begin() + cmdServo.size() + 1, str.end(), r);
            auto ok = (res.ec == std::errc());
            if(ok){
                auto x = clamp(-90.f, r, 90.f);
                if(x != r){ println("Clamped range"); }
                dev::servo::set_rotation_angle(x);
            }else{
                println("Invalid argument to `servo`");
            }
        }else if(str == "debug off"){
            gPrintDebugInfo = false;
        }else if(str == "debug on"){
            gPrintDebugInfo = true;
        }else if(str == "areyouthepico?"){
            println("yes");
        }else{
            println("Unrecognised command. Type `help` for more info.");
        }
    }
}