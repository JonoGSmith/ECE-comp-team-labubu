cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake) # must be before `project()`

project(firmware C CXX ASM)

pico_sdk_init()

# Source files
file(GLOB_RECURSE SRC_C CONFIGURE_DEPENDS "src/*.c")
file(GLOB_RECURSE SRC_CPP CONFIGURE_DEPENDS "src/*.cpp")
add_executable(firmware ${SRC_C} ${SRC_CPP})

pico_set_program_name(firmware "firmware")
pico_set_program_version(firmware "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_usb(firmware 1)
pico_enable_stdio_uart(firmware 0)

target_include_directories(firmware PRIVATE "src/libcfg/")
target_link_libraries(firmware # user libs
    pico_stdlib pico_multicore pico_cyw43_arch_none
    hardware_adc hardware_dma hardware_pwm
    # pico_btstack_ble pico_btstack_cyw43
    # tinyusb_device tinyusb_board
)

pico_add_extra_outputs(firmware)

