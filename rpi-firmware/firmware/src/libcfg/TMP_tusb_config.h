#pragma once
#ifdef __cplusplus
    extern "C" {
#endif
// Configures the on-device usb port as:
// - A device (not host)
// - With a virtual serial port (control packets)
// - And a USB audio class 2 device (mono speaker with mono mic)

// The idea is that the host operating system can read/write audio directly through it's standard audio drivers.
// And then the python server can send control messages to the device via a serial console / packet based format.
// We'll see if it works out that way. If it doesn't, everything could be folded into the packet protocol, but it'd be annoying for Jonathan.
// ---------------------------------------

//--------------------------------------------------------------------+
// Board Specific Configuration
// https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_hid_composite/tusb_config.h
//--------------------------------------------------------------------+

#ifndef BOARD_TUD_RHPORT // RHPort number used for device can be defined by board.mk, default to port 0
    #define BOARD_TUD_RHPORT 0
#endif
#ifndef BOARD_TUD_MAX_SPEED // RHPort max operational speed can defined by board.mk
    #define BOARD_TUD_MAX_SPEED OPT_MODE_DEFAULT_SPEED
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
// https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_stdio_usb/include/pico/stdio_usb.h
// https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_stdio_usb/include/tusb_config.h
// https://github.com/raspberrypi/pico-examples/blob/master/usb/device/dev_hid_composite/tusb_config.h
// https://github.com/pschatzmann/tinyusb/blob/master/examples/device/uac2_speaker_fb/src/tusb_config.h
// https://github.com/pschatzmann/Adafruit_TinyUSB_Arduino/blob/master/src/tusb_config.h
// https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/pico_stdio_usb/stdio_usb_descriptors.c
//--------------------------------------------------------------------


#define CFG_TUD_ENABLED 1   // Operating in device mode, not USB HOST
#ifndef CFG_TUSB_DEBUG
    #define CFG_TUSB_DEBUG  0   // Debug off
#endif
#define CFG_TUD_MAX_SPEED   BOARD_TUD_MAX_SPEED // Default is max speed that hardware controller could support with on-chip PHY

#ifndef CFG_TUSB_OS         // TODO: Why not OPT_OS_PICO, TinyUSB pico mode
    #define CFG_TUSB_OS OPT_OS_NONE
#endif

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
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
    #define CFG_TUD_ENDPOINT0_SIZE 64
#endif

// --- USB DEVICE CLASS --- //
#define CFG_TUD_HID               0
#define CFG_TUD_CDC               1
#define CFG_TUD_MSC               0
#define CFG_TUD_MIDI              0
#define CFG_TUD_VENDOR            0
#define CFG_TUD_AUDIO             0

// CDC FIFO size of TX and RX
#define CFG_TUD_CDC_RX_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_TX_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_CDC_EP_BUFSIZE (TUD_OPT_HIGH_SPEED ? 512 : 64)

// Audio config (speaker)
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN TUD_AUDIO_SPEAKER_MONO_FB_DESC_LEN
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_FORMAT_CORRECTION 0 // Enable if Full-Speed on OSX, also set feedback EP size to 3
#define CFG_TUD_AUDIO_ENABLE_EP_OUT 1
// #define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX        TUD_AUDIO_EP_SIZE(CFG_TUD_AUDIO_FUNC_1_MAX_SAMPLE_RATE, CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX, CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX)
// #define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ     (TUD_OPT_HIGH_SPEED ? 32 : 4) * CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX // Example read FIFO every 1ms, so it should be 8 times larger for HS device
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP 1
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 1
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ 64

#ifdef __cplusplus
    }
#endif