#pragma once
// NOTE:
// The device essentially is acting as a USB composite (a splitter) built out of
// 1. A serial console for controlling the servo and communicating debug information
// 2. A UAC2 compliant speaker (outputs input to the I2S DAC)
// 3. A UAC2 compliant microphone (captures input from the ADC)
// 4. The Raspberry PI reset protocol for convenient development cycles (broken? May be removed soon.)

// NOTE:
// An effort has been made to remove all preprocessor definitions that aren't needed by third party libraries
// And replace them with constants inside `usb_descriptors`

// THANKS
// https://blogs.electro707.com/electronics/2025/02/09/Pico_USB_Audio.html


#ifdef __cplusplus
    extern "C" {
#endif

//--------------------------------------------------------------------+
// Board Specific Configuration
//--------------------------------------------------------------------+

// RHPort number used for device can be defined by board.mk, default to port 0
#ifndef BOARD_TUD_RHPORT
    #define BOARD_TUD_RHPORT      0
#endif
// RHPort max operational speed can defined by board.mk
#ifndef BOARD_TUD_MAX_SPEED
    #define BOARD_TUD_MAX_SPEED   OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// defined by compiler flags for flexibility
#ifndef CFG_TUSB_MCU
    #error CFG_TUSB_MCU must be defined
#endif

#ifndef CFG_TUSB_OS
    #define CFG_TUSB_OS           OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
    #define CFG_TUSB_DEBUG        0
#endif

// USB Device not Host
#define CFG_TUD_ENABLED       1

// Default is max speed that hardware controller could support with on-chip PHY
#define CFG_TUD_MAX_SPEED     BOARD_TUD_MAX_SPEED

#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE)

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
    #define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
    #define CFG_TUSB_MEM_ALIGN __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
    #define CFG_TUD_ENDPOINT0_SIZE    64
#endif

// Supported USB Classes //
#define CFG_TUD_CDC               1
#define CFG_TUD_AUDIO             1
#define CFG_TUD_VENDOR            1

//--------------------------------------------------------------------
// CDC CLASS DRIVER CONFIGURATION
//--------------------------------------------------------------------

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE                    64
#define CFG_TUD_CDC_TX_BUFSIZE                    64

//--------------------------------------------------------------------
// AUDIO DRIVER CONFIGURATION
//--------------------------------------------------------------------
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE                              48000

#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                                 TUD_AUDIO_SPEAKER_MONO_FB_DESC_LEN
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                                 1                                       // Number of Standard AS Interface Descriptors (4.9.1) defined per audio function - this is required to be able to remember the current alternate settings of these interfaces - We restrict us here to have a constant number for all audio functions (which means this has to be the maximum number of AS interfaces an audio function has and a second audio function with less AS interfaces just wastes a few bytes)
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ                              64                                      // Size of control request buffer

#define CFG_TUD_AUDIO_ENABLE_EP_OUT                                   1
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX                    2
#define CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX                            16
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX                            1

#define CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE                          48000

#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX        TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ     (TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX // Example read FIFO every 1ms, so it should be 8 times larger for HS device

#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                             1

#ifdef __cplusplus
    }
#endif

