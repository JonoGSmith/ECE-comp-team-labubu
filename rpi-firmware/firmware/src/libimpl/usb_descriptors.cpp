#include "tusb.h"

extern "C" {
#include "pico/unique_id.h"
#include "pico/stdio_usb/reset_interface.h"
#include "pico/usb_reset_interface.h"
}

#include "../common.hpp"
#include "../endian.hpp"

// #define TUD_AUDIO_SPEAKER_MONO_FB_DESC_LEN (TUD_AUDIO_DESC_IAD_LEN\
//   + TUD_AUDIO_DESC_STD_AC_LEN\
//   + TUD_AUDIO_DESC_CS_AC_LEN\
//   + TUD_AUDIO_DESC_CLK_SRC_LEN\
//   + TUD_AUDIO_DESC_INPUT_TERM_LEN\
//   + TUD_AUDIO_DESC_OUTPUT_TERM_LEN\
//   + TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN\
//   + TUD_AUDIO_DESC_STD_AS_INT_LEN\
//   + TUD_AUDIO_DESC_STD_AS_INT_LEN\
//   + TUD_AUDIO_DESC_CS_AS_INT_LEN\
//   + TUD_AUDIO_DESC_TYPE_I_FORMAT_LEN\
//   + TUD_AUDIO_DESC_STD_AS_ISO_EP_LEN\
//   + TUD_AUDIO_DESC_CS_AS_ISO_EP_LEN\
//   + TUD_AUDIO_DESC_STD_AS_ISO_FB_EP_LEN)


// #define TUD_AUDIO_SPEAKER_MONO_FB_DESCRIPTOR(_itfnum, _stridx, _nBytesPerSample, _nBitsUsedPerSample, _epout, _epoutsize, _epfb, _epfbsize) \
//   /* Standard Interface Association Descriptor (IAD) */\
//   TUD_AUDIO_DESC_IAD(               \
//         _itfnum,   /*_firstitf*/ \
//         0x02,      /*_nitfs*/    \
//         0x00),     /*_stridx*/   \
//   /* Standard AC Interface Descriptor(4.7.1) */\
//   TUD_AUDIO_DESC_STD_AC( \
//     /*_itfnum*/ _itfnum, \
//     /*_nEPs*/ 0x00, \
//     /*_stridx*/ _stridx),\
//   \
//   /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
//   TUD_AUDIO_DESC_CS_AC(     \
//     /*_bcdADC*/ 0x0200, \
//     /*_category*/ AUDIO_FUNC_DESKTOP_SPEAKER, \
//     /*_totallen*/ TUD_AUDIO_DESC_CLK_SRC_LEN+TUD_AUDIO_DESC_INPUT_TERM_LEN+TUD_AUDIO_DESC_OUTPUT_TERM_LEN+TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL_LEN, \
//     /*_ctrl*/ AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
//   /* Clock Source Descriptor(4.7.2.1) */\
//   TUD_AUDIO_DESC_CLK_SRC( \
//     /*_clkid*/ UAC2_ENTITY_CLOCK, \
//     /*_attr*/ AUDIO_CLOCK_SOURCE_ATT_INT_PRO_CLK, \
//     /*_ctrl*/ (AUDIO_CTRL_RW << AUDIO_CLOCK_SOURCE_CTRL_CLK_FRQ_POS), \
//     /*_assocTerm*/ 0x01,  \
//     /*_stridx*/ 0x00),\
//   /* Input Terminal Descriptor(4.7.2.4) */\
//   TUD_AUDIO_DESC_INPUT_TERM(\
//     /*_termid*/ UAC2_ENTITY_INPUT_TERMINAL, \
//     /*_termtype*/ AUDIO_TERM_TYPE_USB_STREAMING, \
//     /*_assocTerm*/ 0x00, \
//     /*_clkid*/ UAC2_ENTITY_CLOCK, \
//     /*_nchannelslogical*/ 0x01, \
//     /*_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, \
//     /*_idxchannelnames*/ 0x00, \
//     /*_ctrl*/ 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), \
//     /*_stridx*/ 0x00),\
//   /* Output Terminal Descriptor(4.7.2.5) */\
//   TUD_AUDIO_DESC_OUTPUT_TERM( \
//     /*_termid*/ UAC2_ENTITY_OUTPUT_TERMINAL, \
//     /*_termtype*/ AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, \
//     /*_assocTerm*/ UAC2_ENTITY_INPUT_TERMINAL, \
//     /*_srcid*/ UAC2_ENTITY_FEATURE_UNIT, \
//     /*_clkid*/ UAC2_ENTITY_CLOCK, \
//     /*_ctrl*/ 0x0000, \
//     /*_stridx*/ 0x00),\
//   /* Feature Unit Descriptor(4.7.2.8) */\
//   TUD_AUDIO_DESC_FEATURE_UNIT_ONE_CHANNEL( \
//     /*_unitid*/ UAC2_ENTITY_FEATURE_UNIT, \
//     /*_srcid*/ UAC2_ENTITY_INPUT_TERMINAL, \
//     /*_ctrlch0master*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, \
//     /*_ctrlch1*/ AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS, \
//     /*_stridx*/ 0x00),\
//   \
//   /* Standard AS Interface Descriptor(4.9.1) */\
//   /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
//   TUD_AUDIO_DESC_STD_AS_INT( \
//     /*_itfnum*/ (uint8_t)((_itfnum) + 1), \
//     /*_altset*/ 0x00, \
//     /*_nEPs*/ 0x00, \
//     /*_stridx*/ 0x00),\
//   /* Standard AS Interface Descriptor(4.9.1) */\
//   /* Interface 1, Alternate 1 - alternate interface for data streaming */\
//   TUD_AUDIO_DESC_STD_AS_INT( \
//     /*_itfnum*/ (uint8_t)((_itfnum) + 1), \
//     /*_altset*/ 0x01, \
//     /*_nEPs*/ 0x02, \
//     /*_stridx*/ 0x00),\
//   /* Class-Specific AS Interface Descriptor(4.9.2) */\
//   TUD_AUDIO_DESC_CS_AS_INT( \
//     /*_termid*/ UAC2_ENTITY_INPUT_TERMINAL, \
//     /*_ctrl*/ AUDIO_CTRL_NONE, \
//     /*_formattype*/ AUDIO_FORMAT_TYPE_I, \
//     /*_formats*/ AUDIO_DATA_FORMAT_TYPE_I_PCM, \
//     /*_nchannelsphysical*/ 0x01, /\
//     *_channelcfg*/ AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, \
//     /*_stridx*/ 0x00),\
//   /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
//   TUD_AUDIO_DESC_TYPE_I_FORMAT(_nBytesPerSample, _nBitsUsedPerSample),\
//   /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
//   TUD_AUDIO_DESC_STD_AS_ISO_EP( \
//     /*_ep*/ _epout, \
//     /*_attr*/ (uint8_t) ((uint8_t)TUSB_XFER_ISOCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (uint8_t)TUSB_ISO_EP_ATT_DATA), \
//     /*_maxEPsize*/ _epoutsize, \
//     /*_interval*/ 0x01),\
//   /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
//   TUD_AUDIO_DESC_CS_AS_ISO_EP( \
//     /*_attr*/ AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, \
//     /*_ctrl*/ AUDIO_CTRL_NONE, \
//     /*_lockdelayunit*/ AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, \
//     /*_lockdelay*/ 0x0001),\
//   /* Standard AS Isochronous Feedback Endpoint Descriptor(4.10.2.1) */\
//   TUD_AUDIO_DESC_STD_AS_ISO_FB_EP( \
//     /*_ep*/ _epfb, \
//     /*_epsize*/ _epfbsize, \
//     /*_interval*/ TUD_OPT_HIGH_SPEED ? 4 : 1)


enum StringDescriptors{
    SD_LANGUAGE = 0,
    SD_MANUFACTURER,
    SD_PRODUCT,
    SD_SERIALNUMBER,
    SD_CDC,
    SD_RPI_RESET,
    SD_UAC_COMPOSITE,
};

//--------------------------------------------------------------------
// ENDPOINTS
//--------------------------------------------------------------------

enum EndpointsOut{
    EPO_AUD = 0x01,
    EPO_CDC = 0x02,
};
enum EndpointsIn{
    EPI_AUD_FB = 0x81,
    EPI_CDC_CMD = 0x81,
    EPI_CDC = 0x82,

};

enum InterfaceIDs{
    ITF_CDC = 0,
    ITF_CDC2, // Implicit (keep directly after CDC)
    ITF_RPI_RESET,
    ITF_AUDIO_CONTROL,
    ITF_AUDIO2, // Implicit (keep directly after AUDIO_CONTROL)
    ITF_COUNTOF
};

//--------------------------------------------------------------------
// PICO RESET
//--------------------------------------------------------------------

#define TUD_RPI_RESET_DESCRIPTOR(_itfnum, _stridx) \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, RESET_INTERFACE_SUBCLASS, RESET_INTERFACE_PROTOCOL, _stridx,

//--------------------------------------------------------------------
// GENERAL
//--------------------------------------------------------------------

static constexpr tusb_desc_device_t usbd_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0110,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor  = 0x2E8A, // Raspberry Pi
    .idProduct = 0x0009, // Raspberry Pi Pico SDK CDC
    .bcdDevice = 0x0100, // Device version (Binary Coded Decimal)
    .iManufacturer = SD_MANUFACTURER,
    .iProduct      = SD_PRODUCT,
    .iSerialNumber = SD_SERIALNUMBER,
    .bNumConfigurations = 1,
};

static constexpr auto usbd_desc_cfg = []()consteval{
    constexpr u16 USBD_MAX_POWER_MA = 250;
    auto temp = std::to_array<u8>({
        TUD_CONFIG_DESCRIPTOR(1, ITF_COUNTOF, 0, /*len*/0, /*attribs*/0, USBD_MAX_POWER_MA),

        TUD_CDC_DESCRIPTOR(ITF_CDC, SD_CDC, EPI_CDC_CMD, 8, EPO_CDC, EPI_CDC, 64),

        TUD_RPI_RESET_DESCRIPTOR(ITF_RPI_RESET, SD_RPI_RESET)

        TUD_AUDIO_SPEAKER_MONO_FB_DESCRIPTOR(ITF_AUDIO_CONTROL,
            SD_UAC_COMPOSITE,
            CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX,
            CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX,
            EPO_AUD,
            CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX,
            EPI_AUD_FB, 4)
    });
    temp[2] = sizeof(temp); // Patch in the correct length
    temp[3] = sizeof(temp) >> 8;
    return temp;
}();

template <size_t N> struct PACKED desc_string {
    u8 length;
    u8 constant_descriptor_type;
    array<u16le, N> str;
};
template <size_t N> consteval auto desc_string_make(array<u16, N> langs){
    auto d = desc_string<N>{};
    d.length = sizeof(d);
    d.constant_descriptor_type = TUSB_DESC_STRING;
    d.str = langs;
    return d;
}
template <size_t N> consteval auto desc_string_make(array<char16_t, N> langs){
    array<u16, N> x = {};
    for(size_t i = 0; i < langs.size(); i++){ x[i] = langs[i]; }
    return desc_string_make(x);
}

// ----------------------------------------------
// Callback Definitions
// -----------------------------------------------

extern "C" u8 const* tud_descriptor_device_cb() {
    return (u8 const*)&usbd_desc_device;
}

extern "C" u8 const* tud_descriptor_configuration_cb(u8 index) {
    return usbd_desc_cfg.begin();
}

extern "C" u16 const* tud_descriptor_string_cb(u8 index, u16 langid) {
    #define X(idx, msg) case idx: { \
        static constexpr auto x = desc_string_make(u ## msg ## _arr); \
        return (u16 const*)&x; \
    }
    switch(index) {
        case SD_LANGUAGE: { // English code
            static constexpr auto x = desc_string_make(array<u16, 1>{0x0409});
            return (u16 const*)&x;
        }
        X(SD_MANUFACTURER, "UniStudents™");
        X(SD_PRODUCT, "SmartDoll");
        X(SD_SERIALNUMBER, "serialIDK");
        X(SD_CDC, "Board CDC");
        X(SD_RPI_RESET, "Pi-Reset");
        X(SD_UAC_COMPOSITE, "Board Audio");
    }
    return nullptr;
}
