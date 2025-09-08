#include "../common.hpp"
#include "usb_handlers.hpp"
#include "../dev/i2s_protocol.hpp"
#include "../dev/i2s_dac.hpp"

#include <stdio.h>
#include "pico/stdlib.h"
#include "bsp/board_api.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"

#include "tusb.h"

#define DEBUG printf

// --------------------------------------
// General USB behaviours
// --------------------------------------

void tud_mount_cb(){}
void tud_umount_cb(){}
void tud_suspend_cb(bool remote_wakeup_en){}
void tud_resume_cb(){}

// --------------------------------------
// The serial protocol backend
// --------------------------------------

void tud_cdc_rx_cb(uint8_t itf){
    // Only one CDC interface exists on the device, so `itf` is ignored.
    static array<u8, 64> buf;
    if(tud_cdc_connected() && tud_cdc_available() > 0) {
        u32 count = tud_cdc_read(buf.begin(), sizeof(buf));
        // Need to process the data now. It may be fragmented
        for(size_t i = 0; i < count; i++) {
            if(buf[i] == '\n') {
                // PROCESS THE LINE HERE
                sv msg = "Helo! I am unda da wata!";
                tud_cdc_write(msg.begin(), msg.size());
                tud_cdc_write_flush();
            }
        }
    }
}

// --------------------------------------
// The audio protocol backend
// --------------------------------------

static array<bool, 1+AUD_SPK_CHANNELS> muteCtrls = {}; // 0: Master, 1: First channel (mono)
static array<s16, 1+AUD_SPK_CHANNELS> volumeCtrls = {};

static uint32_t sampFreq = AUD_SPK_SAMPLE_RATE; // const?
static uint8_t clkValid = 1;

static audio_control_range_2_n_t(1) volumeRng[1+AUD_SPK_CHANNELS]; // Volume range state
static audio_control_range_4_n_t(1) sampleFreqRng = {
    .wNumSubRanges = 1,
    .subrange = {{
        .bMin = AUD_SPK_SAMPLE_RATE,
        .bMax = AUD_SPK_SAMPLE_RATE,
        .bRes = 0,
    }}
}; // Sample frequency range state

enum VOLUM_CTRL {
    VOLUME_CTRL_0_DB   =     0,
    VOLUME_CTRL_10_DB  =  2560,
    VOLUME_CTRL_20_DB  =  5120,
    VOLUME_CTRL_30_DB  =  7680,
    VOLUME_CTRL_40_DB  = 10240,
    VOLUME_CTRL_50_DB  = 12800,
    VOLUME_CTRL_60_DB  = 15360,
    VOLUME_CTRL_70_DB  = 17920,
    VOLUME_CTRL_80_DB  = 20480,
    VOLUME_CTRL_90_DB  = 23040,
    VOLUME_CTRL_100_DB = 25600,
    VOLUME_CTRL_SILENCE = 0x8000,
};

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const* p_request, uint8_t* pBuff) {
    // Page 91 in UAC2 specification
    u8 channelNum = TU_U16_LOW(p_request->wValue);
    u8 ctrlSel    = TU_U16_HIGH(p_request->wValue);
    u8 itf        = TU_U16_LOW(p_request->wIndex);
    u8 entityID   = TU_U16_HIGH(p_request->wIndex);

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // If request is for our feature unit
    if(entityID == TERMID_SPK_FEAT) {
        switch(ctrlSel) {
            case AUDIO_FU_CTRL_MUTE: { // Request uses format layout 1
                TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));
                auto mute = ((audio_control_cur_1_t*)pBuff)->bCur;
                muteCtrls[channelNum] = mute;
                DEBUG("    Set Mute: %d of channel: %u\r\n", mute, channelNum);
                return true;
            }
            case AUDIO_FU_CTRL_VOLUME: { // Request uses format layout 2
                TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));
                auto vol = (uint16_t)((audio_control_cur_2_t*)pBuff)->bCur;
                volumeCtrls[channelNum] = vol;
                DEBUG("    Set channel %d volume: %d dB\r\n", channelNum, vol / 256);
                return true;
            }
            default: // Unknown/Unsupported control
                return false;
        }
    }
    return false; // Yet not implemented
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const* p_request) {
    // Page 91 in UAC2 specification
    u8 channelNum = TU_U16_LOW(p_request->wValue);
    u8 ctrlSel    = TU_U16_HIGH(p_request->wValue);
    u8 itf        = TU_U16_LOW(p_request->wIndex); // Since we have only one audio function implemented, we do not need the itf value
    u8 entityID   = TU_U16_HIGH(p_request->wIndex);

    // Input terminal (Speaker input)
    if(entityID == TERMID_SPK_IN) {
        switch(ctrlSel) {
            case AUDIO_TE_CTRL_CONNECTOR: {
                // The terminal connector control only has a get request with only the CUR attribute.
                static audio_desc_channel_cluster_t ret;
                // Those are dummy values for now
                ret.bNrChannels = 1;
                ret.bmChannelConfig = AUDIO_CHANNEL_CONFIG_FRONT_CENTER;
                ret.iChannelNames = 0;

                DEBUG("    Get terminal connector\r\n");
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*)&ret, sizeof(ret));
            } break;
            default: // Unknown/Unsupported control selector
                return false;
        }
    }

    // Feature unit
    if(entityID == TERMID_SPK_FEAT) {
        switch(ctrlSel) {
            case AUDIO_FU_CTRL_MUTE:
                // Audio control mute cur parameter block consists of only one byte - we thus can send it right away
                // There does not exist a range parameter block for mute
                DEBUG("    Get Mute of channel: %u\r\n", channelNum);
                return tud_control_xfer(rhport, p_request, &muteCtrls[channelNum], sizeof(muteCtrls[0]));
            case AUDIO_FU_CTRL_VOLUME:
                switch(p_request->bRequest) {
                    case AUDIO_CS_REQ_CUR:
                        DEBUG("    Get Volume of channel: %u\r\n", channelNum);
                        return tud_control_xfer(rhport, p_request, &volumeCtrls[channelNum], sizeof(volumeCtrls[0]));
                    case AUDIO_CS_REQ_RANGE: { // Copy values - only for testing - better is version below
                        static audio_control_range_2_n_t(1) ret;

                        ret.wNumSubRanges = tu_htole16(1),
                        ret.subrange[0].bMin = tu_htole16(-VOLUME_CTRL_50_DB);
                        ret.subrange[0].bMax = tu_htole16(VOLUME_CTRL_0_DB);
                        ret.subrange[0].bRes = tu_htole16(256);

                        DEBUG("    Get channel %u volume range (%d, %d, %u) dB\r\n", channelNum,
                          ret.subrange[0].bMin / 256, ret.subrange[0].bMax / 256, ret.subrange[0].bRes / 256);
                        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*)&ret, sizeof(ret));
                    }
                    default: // Unknown/Unsupported control
                        return false;
                } break;
            default: // Unknown/Unsupported control
                return false;
        }
    }

    // Clock Source unit
    if(entityID == TERMID_CLK) {
        switch(ctrlSel) {
            case AUDIO_CS_CTRL_SAM_FREQ:
                // channelNum is always zero in this case
                switch(p_request->bRequest) {
                    case AUDIO_CS_REQ_CUR:
                        DEBUG("    Get Sample Freq.\r\n");
                        return tud_control_xfer(rhport, p_request, &sampFreq, sizeof(sampFreq));
                    case AUDIO_CS_REQ_RANGE:
                        DEBUG("    Get Sample Freq. range\r\n");
                        return tud_control_xfer(rhport, p_request, &sampleFreqRng, sizeof(sampleFreqRng));
                    default: // Unknown/Unsupported control
                        return false;
                }
                break;
            case AUDIO_CS_CTRL_CLK_VALID:
                // Only cur attribute exists for this request
                DEBUG("    Get Sample Freq. valid\r\n");
                return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));
            // Unknown/Unsupported control
            default:
                return false;
        }
    }

    DEBUG("  Unsupported entity: %d\r\n", entityID);
    return false; // Yet not implemented
}

bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting) {
    using namespace dev::dac;
    if (n_bytes_received == 0) return true; // Defensive: if nothing to read, return quickly

    // First chunk: up to end-of-ring
    u32 bytesTillWrap = gAudioRecvBuffer.dist_till_writer_wrap() * sizeof(MonoAudioSampleBE);
    auto bytesToReadFirst = std::min<u32>(bytesTillWrap, n_bytes_received);
    auto bytesPreWrap = tud_audio_read(gAudioRecvBuffer.write_head(), bytesToReadFirst);
    gAudioRecvBuffer.write_reserve_n(bytesPreWrap / sizeof(MonoAudioSampleBE));

    // If there is more remaining (wrap happened), read the rest at ring.begin()
    u16 remaining = n_bytes_received - bytesPreWrap;
    if(remaining){
        auto bytesPostWrap = tud_audio_read(gAudioRecvBuffer.ring.begin(), remaining);
        gAudioRecvBuffer.write_reserve_n(bytesPostWrap / sizeof(MonoAudioSampleBE));
    }

    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const* p_request) {
    return true;
}

void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t* feedback_param) {
    // Set feedback method to fifo counting
    feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    feedback_param->sample_freq = AUD_SPK_SAMPLE_RATE;
}
