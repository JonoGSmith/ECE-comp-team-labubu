#include "../common.hpp"
#include "usb_handlers.hpp"
#include "../dev/i2s_protocol.hpp"
#include "../dev/i2s_dac.hpp"
#include "../dev/mic_adc.hpp"
#include "../console.hpp"

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
    static array<u8, 128> buf;
    static u32 buf_write_head = 0;

    if(tud_cdc_connected() && tud_cdc_available() > 0) {
        // Add the text into the buffer
        buf_write_head += tud_cdc_read(&buf[buf_write_head], sizeof(buf) - buf_write_head);
        // Look for the newlines and process the strings
        {
            u32 front = 0;
            for(size_t i = 0; i < buf_write_head; i++){
                if(buf[i] != '\n') continue;
                auto s = sv{(char*)&buf[front], i - front};
                front = i + 1; // skip \n
                console::processline(s);
            }
            if(front == 0 && buf_write_head == sizeof(buf)){ // Traversed the whole buffer without finding a newline
                sv msg = "Message too long. Ignored";
                tud_cdc_write(msg.begin(), msg.size());
                tud_cdc_write_flush();
            }
            // Move the remaining characters into the front of the buf in prep for the next recv.
            bool buf_full = front >= sizeof(buf);
            auto buf_remainder = buf_write_head - front;
            if(buf_full){
                std::copy_n(buf.begin() + front, buf_remainder, buf.begin());
            }
            buf_write_head = buf_full ? 0 : buf_remainder;
        }
    }
}

// --------------------------------------
// The audio protocol backend
// --------------------------------------

enum VOLUME_CTRL {
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

static array<bool, 1+AUD_SPK_CHANNELS> muteCtrls = {}; // 0: Master, 1: First channel (mono)
static array<s16, 1+AUD_SPK_CHANNELS> volumeCtrls = {};

// List of supported sample rates
constexpr auto sample_rates = std::to_array<u32>({48000});
uint32_t current_sample_rate = sample_rates[0];

// Helper for feature unit set requests
static bool audio_feature_unit_set_request(uint8_t rhport, audio_control_request_t const *request, uint8_t const *buf) {
    TU_ASSERT(request->bEntityID == TERMID_SPK_FEAT);
    TU_VERIFY(request->bRequest == AUDIO_CS_REQ_CUR);

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE) {
        TU_VERIFY(request->wLength == sizeof(audio_control_cur_1_t));
        muteCtrls[request->bChannelNumber] = ((audio_control_cur_1_t const *) buf)->bCur;
        DEBUG("Set channel %d Mute: %d\r\n", request->bChannelNumber, muteCtrls[request->bChannelNumber]);
        return true;
    } else if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
        TU_VERIFY(request->wLength == sizeof(audio_control_cur_2_t));
        volumeCtrls[request->bChannelNumber] = ((audio_control_cur_2_t const *) buf)->bCur;
        DEBUG("Set channel %d volume: %d dB\r\n", request->bChannelNumber, volumeCtrls[request->bChannelNumber] / 256);
        return true;
    } else {
        DEBUG("Feature unit set request not supported, entity = %u, selector = %u, request = %u\r\n",
                request->bEntityID, request->bControlSelector, request->bRequest);
        return false;
    }
}


// Helper for clock set requests
static bool audio_clock_set_request(uint8_t rhport, audio_control_request_t const *request, uint8_t const *buf) {
    TU_ASSERT(request->bEntityID == TERMID_CLK);
    TU_VERIFY(request->bRequest == AUDIO_CS_REQ_CUR);

    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        TU_VERIFY(request->wLength == sizeof(audio_control_cur_4_t));
        current_sample_rate = (uint32_t) ((audio_control_cur_4_t const *) buf)->bCur;
        DEBUG("Clock set current freq: %" PRIu32 "\r\n", current_sample_rate);
        return true;
    } else {
        DEBUG("Clock set request not supported, entity = %u, selector = %u, request = %u\r\n",
                request->bEntityID, request->bControlSelector, request->bRequest);
        return false;
    }
}

// Helper for clock get requests
static bool audio_clock_get_request(uint8_t rhport, audio_control_request_t const *request) {
    TU_ASSERT(request->bEntityID == TERMID_CLK);

    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        if (request->bRequest == AUDIO_CS_REQ_CUR) {
            DEBUG("Clock get current freq %" PRIu32 "\r\n", current_sample_rate);

            audio_control_cur_4_t curf = {(int32_t) tu_htole32(current_sample_rate)};
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &curf, sizeof(curf));
        } else if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_4_n_t(sample_rates.size()) rangef = {.wNumSubRanges = tu_htole16(sample_rates.size())};
            DEBUG("Clock get %d freq ranges\r\n", sample_rates.size());
            for (uint8_t i = 0; i < sample_rates.size(); i++) {
                rangef.subrange[i].bMin = (int32_t) sample_rates[i];
                rangef.subrange[i].bMax = (int32_t) sample_rates[i];
                rangef.subrange[i].bRes = 0;
                DEBUG("Range %d (%d, %d, %d)\r\n", i, (int) rangef.subrange[i].bMin, (int) rangef.subrange[i].bMax, (int) rangef.subrange[i].bRes);
            }

            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &rangef, sizeof(rangef));
        }
    } else if (request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t cur_valid = {.bCur = 1};
        DEBUG("Clock get is valid %u\r\n", cur_valid.bCur);
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &cur_valid, sizeof(cur_valid));
    }
    DEBUG("Clock get request not supported, entity = %u, selector = %u, request = %u\r\n",
            request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
}

// Helper for feature unit get requests
static bool audio_feature_unit_get_request(uint8_t rhport, audio_control_request_t const *request) {
    TU_ASSERT(request->bEntityID == TERMID_SPK_FEAT);

    if (request->bControlSelector == AUDIO_FU_CTRL_MUTE && request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t mute1 = {.bCur = muteCtrls[request->bChannelNumber]};
        DEBUG("Get channel %u mute %d\r\n", request->bChannelNumber, mute1.bCur);
        return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &mute1, sizeof(mute1));
    } else if (request->bControlSelector == AUDIO_FU_CTRL_VOLUME) {
        if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_2_n_t(1) range_vol = {
                .wNumSubRanges = tu_htole16(1),
                .subrange = {{.bMin = tu_htole16(-VOLUME_CTRL_50_DB), .bMax = tu_htole16(VOLUME_CTRL_0_DB), .bRes = tu_htole16(256)}}};
            DEBUG("Get channel %u volume range (%d, %d, %u) dB\r\n", request->bChannelNumber,
                    range_vol.subrange[0].bMin / 256, range_vol.subrange[0].bMax / 256, range_vol.subrange[0].bRes / 256);
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &range_vol, sizeof(range_vol));
        } else if (request->bRequest == AUDIO_CS_REQ_CUR) {
            audio_control_cur_2_t cur_vol = {.bCur = tu_htole16(volumeCtrls[request->bChannelNumber])};
            DEBUG("Get channel %u volume %d dB\r\n", request->bChannelNumber, cur_vol.bCur / 256);
            return tud_audio_buffer_and_schedule_control_xfer(rhport, (tusb_control_request_t const *) request, &cur_vol, sizeof(cur_vol));
        }
    }
    DEBUG("Feature unit get request not supported, entity = %u, selector = %u, request = %u\r\n",
            request->bEntityID, request->bControlSelector, request->bRequest);

    return false;
}

// --------------------------------------
// The audio protocol CALLBACKS
// --------------------------------------

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
    auto request = (audio_control_request_t const *) p_request;

    if (request->bEntityID == TERMID_SPK_FEAT)
        return audio_feature_unit_set_request(rhport, request, buf);
    if (request->bEntityID == TERMID_CLK)
        return audio_clock_set_request(rhport, request, buf);

    DEBUG("Set request not handled, entity = %d, selector = %d, request = %d\r\n",
          request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    auto request = (audio_control_request_t const *) p_request;

    if (request->bEntityID == TERMID_CLK)
        return audio_clock_get_request(rhport, request);
    if (request->bEntityID == TERMID_SPK_FEAT)
        return audio_feature_unit_get_request(rhport, request);

    DEBUG("Get request not handled, entity = %d, selector = %d, request = %d\r\n",
            request->bEntityID, request->bControlSelector, request->bRequest);
    return false;
}

// The mythical audio receive function
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

// Stubbed set interface and interface close. We just pump all the time to one.
bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    return true;
}
bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const* p_request) {
    return true;
}