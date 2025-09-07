#include "tusb.h"

extern "C" {
#include "pico/unique_id.h"
#include "pico/stdio_usb/reset_interface.h"
#include "pico/usb_reset_interface.h"
}

#include "../common.hpp"
#include "../endian.hpp"

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
    EPO_CDC = 0x02,
    EPO_AUD = 0x03,
};
enum EndpointsIn{
    EPI_CDC_CMD = 0x81,
    EPI_CDC = 0x82,
    EPI_AUD_FB = 0x83,
};

enum InterfaceIDs{
    ITF_CDC = 0,
    ITF_CDC_DATA, // Implicit (keep directly after CDC)
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
    .bcdUSB = 0x0200, // TODO: All examples are using this, but its wrong? USB 1.1?
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

constexpr bool attribBusPowered = true;
constexpr bool attribSelfPowered = false;
constexpr u8 configAttribs = (attribBusPowered << 7) | (attribSelfPowered << 6);

static constexpr auto usbd_desc_cfg = []()consteval{
    constexpr u16 USBD_MAX_POWER_MA = 250;
    auto temp = std::to_array<u8>({
        TUD_CONFIG_DESCRIPTOR(1, ITF_COUNTOF, 0, /*len*/0, configAttribs, USBD_MAX_POWER_MA),

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
        X(SD_MANUFACTURER, "UniStudentsTM");
        X(SD_PRODUCT, "SmartDoll");
        X(SD_SERIALNUMBER, "E409053RealNumber");
        X(SD_CDC, "Board CDC");
        X(SD_RPI_RESET, "Pi-Reset");
        X(SD_UAC_COMPOSITE, "Board Audio");
    }
    return nullptr;
}
