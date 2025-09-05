#include "tusb.h"
#include "usb_descriptors.h"
#include "pico/unique_id.h"

#include "pico/stdio_usb/reset_interface.h"
#include "pico/usb_reset_interface.h"

static const tusb_desc_device_t usbd_desc_device = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
// On Windows, if bcdUSB = 0x210 then a Microsoft OS 2.0 descriptor is required, else the device won't be detected
// This is only needed for driverless access to the reset interface - the CDC interface doesn't require these descriptors
// for driverless access, but will still not work if bcdUSB = 0x210 and no descriptor is provided. Therefore always
// use bcdUSB = 0x200 if the Microsoft OS 2.0 descriptor isn't enabled
#if PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE && PICO_STDIO_USB_RESET_INTERFACE_SUPPORT_MS_OS_20_DESCRIPTOR
    .bcdUSB = 0x0210,
#else
    .bcdUSB = 0x0200,
#endif
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = USBD_VID,
    .idProduct = USBD_PID,
    .bcdDevice = 0x0100,
    .iManufacturer = 0x01,
    .iProduct = 0x02,
    .iSerialNumber = 0x03,
    .bNumConfigurations = 1,
};

static const uint8_t usbd_desc_cfg[USBD_DESC_LEN] = {
    TUD_CONFIG_DESCRIPTOR(1, USBD_ITF_MAX, 0, USBD_DESC_LEN,
        USBD_CONFIGURATION_DESCRIPTOR_ATTRIBUTE, USBD_MAX_POWER_MA),

    TUD_CDC_DESCRIPTOR(USBD_ITF_CDC, 4, USBD_CDC_EP_CMD,
        USBD_CDC_CMD_MAX_SIZE, USBD_CDC_EP_OUT, USBD_CDC_EP_IN, USBD_CDC_IN_OUT_MAX_SIZE),

    // interface number, string index
    TUD_RPI_RESET_DESCRIPTOR(USBD_ITF_RPI_RESET, USBD_STR_RPI_RESET)

    // TUD_AUDIO_MIC_ONE_CH_DESCRIPTOR(/*_itfnum*/ ITF_NUM_AUDIO_CONTROL, /*_stridx*/ 0, /*_nBytesPerSample*/ CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX, /*_nBitsUsedPerSample*/ CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX*8,/*_epin*/ EPNUM_AUDIO, /*_epsize*/ CFG_TUD_AUDIO_EP_SZ_IN)
    TUD_AUDIO_SPEAKER_MONO_FB_DESCRIPTOR(
        ITF_NUM_AUDIO_CONTROL,
        6,
        CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX,
        CFG_TUD_AUDIO_FUNC_1_RESOLUTION_RX,
        EPNUM_AUDIO_OUT,
        CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX,
        EPNUM_AUDIO_FB, 4),
};

const uint8_t *tud_descriptor_device_cb(void) {
    return (const uint8_t *)&usbd_desc_device;
}

const uint8_t *tud_descriptor_configuration_cb(__unused uint8_t index) {
    return usbd_desc_cfg;
}

static char usbd_serial_str[PICO_UNIQUE_BOARD_ID_SIZE_BYTES * 2 + 1];
static const char *const usbd_desc_str[] = {
    [1] = USBD_MANUFACTURER,
    [2] = USBD_PRODUCT,
    [3] = usbd_serial_str,
    [4] = "Board CDC",
    [5] = "Pi-Reset",
    [6] = "UAC2"
};

const uint16_t *tud_descriptor_string_cb(uint8_t index, __unused uint16_t langid) {
#ifndef USBD_DESC_STR_MAX
#define USBD_DESC_STR_MAX (20)
#elif USBD_DESC_STR_MAX > 127
#error USBD_DESC_STR_MAX too high (max is 127).
#elif USBD_DESC_STR_MAX < 17
#error USBD_DESC_STR_MAX too low (min is 17).
#endif
    static uint16_t desc_str[USBD_DESC_STR_MAX];

    // Assign the SN using the unique flash id
    if (!usbd_serial_str[0]) {
        pico_get_unique_board_id_string(usbd_serial_str, sizeof(usbd_serial_str));
    }

    uint8_t len;
    if (index == 0) {
        desc_str[1] = 0x0409; // supported language is English
        len = 1;
    } else {
        if (index >= sizeof(usbd_desc_str) / sizeof(usbd_desc_str[0])) {
            return NULL;
        }
        const char *str = usbd_desc_str[index];
        for (len = 0; len < USBD_DESC_STR_MAX - 1 && str[len]; ++len) {
            desc_str[1 + len] = str[len];
        }
    }

    // first byte is length (including header), second byte is string type
    desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * len + 2));

    return desc_str;
}