#include "tusb.h"

extern "C" {
#include "pico/unique_id.h"
#include "pico/stdio_usb/reset_interface.h"
#include "pico/usb_reset_interface.h"
}

#include "../common.hpp"
#include "../endian.hpp"
#include "usb_handlers.hpp"

enum StringDescriptors{
    SD_LANGUAGE = 0,
    SD_MANUFACTURER,
    SD_PRODUCT,
    SD_SERIALNUMBER,
    SD_CDC,
    SD_RPI_RESET,
    SD_UAC_UAC2,
    SD_UAC_SPEAKER,
    SD_UAC_MICROPHONE,
};

//--------------------------------------------------------------------
// ENDPOINTS
//--------------------------------------------------------------------

enum EndpointsOut{
    EPO_CDC = 0x01,
    EPO_AUD = 0x04,
};
enum EndpointsIn{
    EPI_CDC = 0x81,
    EPI_CDC_CMD = 0x82,
    EPI_AUD_INT = 0x83,
    EPI_AUD = 0x84,
    EPI_AUD_FB = 0x85,
};

enum InterfaceIDs{
    ITF_CDC = 0,
    ITF_CDC_DATA, // Implicit (keep directly after CDC)
    ITF_RPI_RESET,
    ITF_AUDIO_CONTROL,
    ITF_AUDIO_SPEAKER,
    ITF_AUDIO_MICROPHONE,
    ITF_COUNTOF
};

//--------------------------------------------------------------------
// PICO RESET
//--------------------------------------------------------------------

#define TUD_RPI_RESET_DESCRIPTOR(_itfnum, _stridx) \
    9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, RESET_INTERFACE_SUBCLASS, RESET_INTERFACE_PROTOCOL, _stridx

//--------------------------------------------------------------------
// GENERAL
//--------------------------------------------------------------------

static constexpr tusb_desc_device_t usbd_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200, // But FullSpeed is the max
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor  = 0x2E8A, // Raspberry Pi
    .idProduct = 0x0010, // Raspberry Pi Pico SDK CDC
    .bcdDevice = 0x0100, // Device version (Binary Coded Decimal)
    .iManufacturer = SD_MANUFACTURER,
    .iProduct      = SD_PRODUCT,
    .iSerialNumber = SD_SERIALNUMBER,
    .bNumConfigurations = 1,
};

#define NO_STR 0

// Descritpors for the desktop speaker
#define UAC2_DESCRIPTORS_B \
    /* Clock Source Descriptor(4.7.2.1) */\
    TUD_AUDIO_DESC_CLK_SRC(TERMID_CLK, /*_attr*/ 3, /*_ctrl*/ 7, /*_assocTerm*/ 0x00,  NO_STR),    \
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO_DESC_INPUT_TERM(TERMID_SPK_IN, AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, TERMID_CLK, /*_nchannelslogical TODO*/ 2, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, NO_STR, /*_ctrl*/ 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), NO_STR),\
    /* Feature Unit Descriptor(4.7.2.8) */\
    TUD_AUDIO_DESC_FEATURE_UNIT_TWO_CHANNEL(TERMID_SPK_FEAT, TERMID_SPK_IN, /*_ctrlch0master*/ (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch1*/ (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS), /*_ctrlch2*/ (AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_MUTE_POS | AUDIO_CTRL_RW << AUDIO_FEATURE_UNIT_CTRL_VOLUME_POS), NO_STR),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO_DESC_OUTPUT_TERM(TERMID_SPK_OUT, AUDIO_TERM_TYPE_OUT_DESKTOP_SPEAKER, /*_assocTerm*/ 0x00, TERMID_SPK_FEAT, TERMID_CLK, /*_ctrl*/ 0, NO_STR),\
    /* Input Terminal Descriptor(4.7.2.4) */\
    TUD_AUDIO_DESC_INPUT_TERM(TERMID_MIC_IN, AUDIO_TERM_TYPE_IN_GENERIC_MIC, /*_assocTerm*/ 0x00, TERMID_CLK, /*_nchannelslogical*/ 1, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, NO_STR, /*_ctrl*/ 0 * (AUDIO_CTRL_R << AUDIO_IN_TERM_CTRL_CONNECTOR_POS), NO_STR),\
    /* Output Terminal Descriptor(4.7.2.5) */\
    TUD_AUDIO_DESC_OUTPUT_TERM(TERMID_MIC_OUT, AUDIO_TERM_TYPE_USB_STREAMING, /*_assocTerm*/ 0x00, TERMID_MIC_IN, TERMID_CLK, /*_ctrl*/ 0, NO_STR)\

#define UAC2_DESCRIPTORS(_stridx, _epout, _epin, _epint) \
    /* Standard Interface Association Descriptor (IAD): Tells the host to strongly group UAC, Speaker, & Mic interfaces*/\
    TUD_AUDIO_DESC_IAD(ITF_AUDIO_CONTROL, 3, NO_STR),\
    /* Standard AC Interface Descriptor(4.7.1) */\
    TUD_AUDIO_DESC_STD_AC(ITF_AUDIO_CONTROL, /*_nEPs*/ 1, _stridx),\
    /* Class-Specific AC Interface Header Descriptor(4.7.2) */\
    TUD_AUDIO_DESC_CS_AC(0x0200, AUDIO_FUNC_HEADSET, sizeof(std::to_array<u8>({UAC2_DESCRIPTORS_B})), AUDIO_CS_AS_INTERFACE_CTRL_LATENCY_POS),\
    UAC2_DESCRIPTORS_B, \
    /* Standard AC Interrupt Endpoint Descriptor(4.8.2.1) */\
    TUD_AUDIO_DESC_STD_AC_INT_EP(/*_ep*/ _epint, /*_interval*/ 0x01), \
    \
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO_DESC_STD_AS_INT(ITF_AUDIO_SPEAKER, /*_altset*/ 0, /*_nEPs*/ 0, SD_UAC_SPEAKER),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 1, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO_DESC_STD_AS_INT(ITF_AUDIO_SPEAKER, /*_altset*/ 1, /*_nEPs*/ 1, SD_UAC_SPEAKER),\
        /* Class-Specific AS Interface Descriptor(4.9.2) */\
        TUD_AUDIO_DESC_CS_AS_INT(TERMID_SPK_IN, AUDIO_CTRL_NONE, AUDIO_FORMAT_TYPE_I, AUDIO_DATA_FORMAT_TYPE_I_PCM, AUD_SPK_CHANNELS, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, NO_STR),\
        /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
        TUD_AUDIO_DESC_TYPE_I_FORMAT(AUD_SPK_BYTES_PER_SAMPLE, AUD_SPK_BITS_PER_SAMPLE),\
        /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
        TUD_AUDIO_DESC_STD_AS_ISO_EP(_epout, ((u8)TUSB_XFER_ISOCHRONOUS | (u8)TUSB_ISO_EP_ATT_ADAPTIVE | (u8)TUSB_ISO_EP_ATT_DATA), CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX, /*_interval*/ 0x01),\
        /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
        TUD_AUDIO_DESC_CS_AS_ISO_EP(AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, AUDIO_CTRL_NONE, AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_MILLISEC, /*_lockdelay*/ 1),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 0 - default alternate setting with 0 bandwidth */\
    TUD_AUDIO_DESC_STD_AS_INT(ITF_AUDIO_MICROPHONE, /*_altset*/ 0, /*_nEPs*/ 0, SD_UAC_MICROPHONE),\
    /* Standard AS Interface Descriptor(4.9.1) */\
    /* Interface 2, Alternate 1 - alternate interface for data streaming */\
    TUD_AUDIO_DESC_STD_AS_INT(ITF_AUDIO_MICROPHONE, /*_altset*/ 1, /*_nEPs*/ 1, SD_UAC_MICROPHONE),\
        /* Class-Specific AS Interface Descriptor(4.9.2) */\
        TUD_AUDIO_DESC_CS_AS_INT(TERMID_MIC_OUT, AUDIO_CTRL_NONE, AUDIO_FORMAT_TYPE_I, AUDIO_DATA_FORMAT_TYPE_I_PCM, AUD_MIC_CHANNELS, AUDIO_CHANNEL_CONFIG_NON_PREDEFINED, NO_STR),\
        /* Type I Format Type Descriptor(2.3.1.6 - Audio Formats) */\
        TUD_AUDIO_DESC_TYPE_I_FORMAT(AUD_MIC_BYTES_PER_SAMPLE, AUD_MIC_BITS_PER_SAMPLE),\
        /* Standard AS Isochronous Audio Data Endpoint Descriptor(4.10.1.1) */\
        TUD_AUDIO_DESC_STD_AS_ISO_EP(_epin, ((u8)TUSB_XFER_ISOCHRONOUS | (u8)TUSB_ISO_EP_ATT_ASYNCHRONOUS | (u8)TUSB_ISO_EP_ATT_DATA), CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX, /*_interval*/ 0x01),\
        /* Class-Specific AS Isochronous Audio Data Endpoint Descriptor(4.10.1.2) */\
        TUD_AUDIO_DESC_CS_AS_ISO_EP(AUDIO_CS_AS_ISO_DATA_EP_ATT_NON_MAX_PACKETS_OK, AUDIO_CTRL_NONE, AUDIO_CS_AS_ISO_DATA_EP_LOCK_DELAY_UNIT_UNDEFINED, /*_lockdelay*/ 0)


constexpr bool attribBusPowered = true;
constexpr bool attribSelfPowered = false;
constexpr u8 configAttribs = (attribBusPowered << 7) | (attribSelfPowered << 6);
static_assert(sizeof(std::to_array<u8>({UAC2_DESCRIPTORS(SD_UAC_UAC2, EPO_AUD, EPI_AUD, EPI_AUD_INT)})) == CFG_TUD_AUDIO_FUNC_1_DESC_LEN, "These must match (update tusb_config)");

static constexpr auto usbd_desc_cfg = []()consteval{
    constexpr u16 USBD_MAX_POWER_MA = 250;
    auto temp = std::to_array<u8>({
        TUD_CONFIG_DESCRIPTOR(1, ITF_COUNTOF, 0, /*len*/0, configAttribs, USBD_MAX_POWER_MA),
        TUD_CDC_DESCRIPTOR(ITF_CDC, SD_CDC, EPI_CDC_CMD, 8, EPO_CDC, EPI_CDC, 64),
        TUD_RPI_RESET_DESCRIPTOR(ITF_RPI_RESET, SD_RPI_RESET),
        UAC2_DESCRIPTORS(SD_UAC_UAC2, EPO_AUD, EPI_AUD, EPI_AUD_INT),
    });
    temp[2] = sizeof(temp) & 0xff; // Patch in the correct length
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
        X(SD_MANUFACTURER, "UniStudentsTM");
        X(SD_PRODUCT, "SmartDoll");
        X(SD_SERIALNUMBER, "E409053RealNumber");
        X(SD_CDC, "Board CDC");
        X(SD_RPI_RESET, "Pi-Reset");
        X(SD_UAC_UAC2, "UAC Compliant Device");
        X(SD_UAC_SPEAKER, "UAC Speaker");
        X(SD_UAC_MICROPHONE, "UAC Microphone");
    }
    return nullptr;
}
