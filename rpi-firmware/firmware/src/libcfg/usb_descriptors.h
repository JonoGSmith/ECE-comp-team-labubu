#ifndef USB_DESCRIPTORS_H_
#define USB_DESCRIPTORS_H_

// #include "tusb.h"

//--------------------------------------------------------------------
// AUDIO
//--------------------------------------------------------------------

// Defined in TUD_AUDIO_SPEAKER_STEREO_FB_DESCRIPTOR
#define UAC2_ENTITY_CLOCK               0x04
#define UAC2_ENTITY_INPUT_TERMINAL      0x01
#define UAC2_ENTITY_FEATURE_UNIT        0x02
#define UAC2_ENTITY_OUTPUT_TERMINAL     0x03


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

//--------------------------------------------------------------------
// ENDPOINTS
//--------------------------------------------------------------------

#define USBD_CDC_EP_CMD (0x81)
#define USBD_CDC_EP_OUT (0x02)
#define USBD_CDC_EP_IN (0x82)
#define USBD_CDC_CMD_MAX_SIZE (8)
#define USBD_CDC_IN_OUT_MAX_SIZE (64)

#define USBD_STR_RPI_RESET (0x05)

#define EPNUM_AUDIO_FB    0x81
#define EPNUM_AUDIO_OUT   0x01

//--------------------------------------------------------------------
// INTERFACES
//--------------------------------------------------------------------

#define USBD_ITF_CDC       0
// the above ITF will create another one
#define USBD_ITF_RPI_RESET 2
#define ITF_NUM_AUDIO_CONTROL 3
// the above ITF will create another one

//--------------------------------------------------------------------
// PICO RESET
//--------------------------------------------------------------------

#define TUD_RPI_RESET_DESC_LEN  9

#define TUD_RPI_RESET_DESCRIPTOR(_itfnum, _stridx) \
  /* Interface */\
  9, TUSB_DESC_INTERFACE, _itfnum, 0, 0, TUSB_CLASS_VENDOR_SPECIFIC, RESET_INTERFACE_SUBCLASS, RESET_INTERFACE_PROTOCOL, _stridx,

//--------------------------------------------------------------------
// GENERAL
//--------------------------------------------------------------------

#define USBD_VID (0x2E8A) // Raspberry Pi
#define USBD_PID (0x0009) // Raspberry Pi Pico SDK CDC

#define USBD_MANUFACTURER "Raspberry Pi"
#define USBD_PRODUCT "Pico"

#define USBD_DESC_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_RPI_RESET_DESC_LEN + TUD_AUDIO_SPEAKER_MONO_FB_DESC_LEN)
#define USBD_MAX_POWER_MA (250)

#define USBD_ITF_MAX (5)
#define USBD_CONFIGURATION_DESCRIPTOR_ATTRIBUTE (0)


#endif