/**
 * RP2040 USB Audio Example
 * By Jamal Bouajja
 *
 * This software is a demonstration for a USB audio
 *
 * Parts of this are hacked together, so execuse the clutter
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "bsp/board_api.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"

#include "tusb.h"
#include "usb_descriptors.h"

#define DEBUG printf

enum {
    BLINK_NOT_MOUNTED = 250,
    BLINK_MOUNTED = 1000,
    BLINK_SUSPENDED = 2500,
};
enum {
    VOLUME_CTRL_0_DB = 0,
    VOLUME_CTRL_10_DB = 2560,
    VOLUME_CTRL_20_DB = 5120,
    VOLUME_CTRL_30_DB = 7680,
    VOLUME_CTRL_40_DB = 10240,
    VOLUME_CTRL_50_DB = 12800,
    VOLUME_CTRL_60_DB = 15360,
    VOLUME_CTRL_70_DB = 17920,
    VOLUME_CTRL_80_DB = 20480,
    VOLUME_CTRL_90_DB = 23040,
    VOLUME_CTRL_100_DB = 25600,
    VOLUME_CTRL_SILENCE = 0x8000,
};

#define SER_RX_FIFO_LEN 64

#define PWM_PIN   15
#define DEBUG_PIN 14

#define PWM_AUDIO_INT_SLICE 0
#define PWM_OUTPUT_SLICE    7

#define SPEAKER_BUFF_SIZE 2048 // must be power of two
#define SPEAKER_BUFF_AND  (SPEAKER_BUFF_SIZE - 1)


static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

bool mute[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1];      // +1 for master channel 0
int16_t volume[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1]; // +1 for master channel 0
uint32_t sampFreq;
uint8_t clkValid;

audio_control_range_2_n_t(1) volumeRng[CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX + 1]; // Volume range state
audio_control_range_4_n_t(1) sampleFreqRng;                                     // Sample frequency range state

// Audio test data
// int16_t spk_buf[CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ / 4];
int16_t spk_buf[SPEAKER_BUFF_SIZE];
uint16_t spk_bufIn = 0;
uint16_t spk_bufOut = 0;

void led_blinking_task(void);
void processLine(uint8_t* token);

void audioPwmWrap(void);

int main() {
    // gpio_init(PICO_DEFAULT_LED_PIN);
    // gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    // stdio_init_all();
    board_init();

    tusb_init();

    // Init values
    sampFreq = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    clkValid = 1;

    DEBUG("START");

    sampleFreqRng.wNumSubRanges = 1;
    sampleFreqRng.subrange[0].bMin = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bMax = CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE;
    sampleFreqRng.subrange[0].bRes = 0;

    gpio_set_function(PWM_PIN, GPIO_FUNC_PWM);
    gpio_init(DEBUG_PIN);
    gpio_set_dir(DEBUG_PIN, GPIO_OUT);

    pwm_clear_irq(PWM_AUDIO_INT_SLICE);
    pwm_set_irq_enabled(PWM_AUDIO_INT_SLICE, true);
    pwm_set_wrap(PWM_AUDIO_INT_SLICE, 2625 - 1);
    pwm_set_clkdiv_int_frac4(PWM_AUDIO_INT_SLICE, 1, 0);
    pwm_set_enabled(PWM_AUDIO_INT_SLICE, true);

    pwm_set_clkdiv_int_frac4(PWM_OUTPUT_SLICE, 1, 0);
    pwm_set_chan_level(PWM_OUTPUT_SLICE, PWM_CHAN_B, 0x1FF);
    pwm_set_wrap(PWM_OUTPUT_SLICE, 0x3FF);
    pwm_set_enabled(PWM_OUTPUT_SLICE, true);

    irq_set_exclusive_handler(PWM_IRQ_WRAP, audioPwmWrap);
    irq_set_priority(PWM_IRQ_WRAP, 0);
    irq_set_priority(USBCTRL_IRQ, 10);

    irq_set_enabled(PWM_DEFAULT_IRQ_NUM(), true);

    while(true) {
        tud_task(); // TinyUSB device task
        led_blinking_task();
    }
}


void led_blinking_task(void) {
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // blink is disabled
    if(!blink_interval_ms) return;

    // Blink every interval ms
    if(board_millis() - start_ms < blink_interval_ms) return; // not enough time
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state; // toggle
}

#if CFG_TUD_CDC
void tud_cdc_rx_cb(uint8_t itf) {
    static uint8_t buf[128];
    static uint32_t bufIdx = 0;
    uint32_t count;

    // connected() check for DTR bit
    // Most but not all terminal client set this when making connection
    if(tud_cdc_connected()) {
        if(tud_cdc_available()) // data is available
        {
            // todo, if above buffer allocation, handle plz
            count = tud_cdc_n_read(itf, buf + bufIdx, 128 - bufIdx);

            for(int i = bufIdx; i < bufIdx + count; i++) {
                if(buf[i] == '\n') {
                    processLine(buf);
                    bufIdx = 0;
                    count = 0;
                    break;
                }
            }

            bufIdx += count;

            //   tud_cdc_n_write(itf, buf, count);
            //   tud_cdc_n_write_flush(itf);
            // dummy code to check that cdc serial is responding
            //   printf("Responding!\n");
        }
    }
}

void processLine(uint8_t* token) {
    // printf("Processing Line %s", token);
    if(strcmp(token, "ping")) {
        tud_cdc_n_write(0, "pong!", 5);
    }
    tud_cdc_n_write_flush(0);
}
#endif

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const* p_request, uint8_t* pBuff) {
    (void)rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t itf = TU_U16_LOW(p_request->wIndex);
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    (void)itf;

    // We do not support any set range requests here, only current value requests
    TU_VERIFY(p_request->bRequest == AUDIO_CS_REQ_CUR);

    // If request is for our feature unit
    if(entityID == UAC2_ENTITY_FEATURE_UNIT) {
        switch(ctrlSel) {
            case AUDIO_FU_CTRL_MUTE:
                // Request uses format layout 1
                TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_1_t));

                mute[channelNum] = ((audio_control_cur_1_t*)pBuff)->bCur;

                DEBUG("    Set Mute: %d of channel: %u\r\n", mute[channelNum], channelNum);
                return true;

            case AUDIO_FU_CTRL_VOLUME:
                // Request uses format layout 2
                TU_VERIFY(p_request->wLength == sizeof(audio_control_cur_2_t));

                volume[channelNum] = (uint16_t)((audio_control_cur_2_t*)pBuff)->bCur;

                DEBUG("    Set channel %d volume: %d dB\r\n", channelNum, volume[channelNum] / 256);
                return true;

                // Unknown/Unsupported control
            default:
                TU_BREAKPOINT();
                return false;
        }
    }
    return false; // Yet not implemented
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const* p_request) {
    (void)rhport;

    // Page 91 in UAC2 specification
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    // uint8_t itf = TU_U16_LOW(p_request->wIndex); 			// Since we have only one audio function implemented, we do not need the itf value
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);

    // Input terminal (Speaker input)
    if(entityID == UAC2_ENTITY_INPUT_TERMINAL) {
        switch(ctrlSel) {
            case AUDIO_TE_CTRL_CONNECTOR: {
                // The terminal connector control only has a get request with only the CUR attribute.
                audio_desc_channel_cluster_t ret;

                // Those are dummy values for now
                ret.bNrChannels = 1;
                ret.bmChannelConfig = (audio_channel_config_t)0;
                ret.iChannelNames = 0;

                DEBUG("    Get terminal connector\r\n");

                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*)&ret, sizeof(ret));
            } break;

                // Unknown/Unsupported control selector
            default:
                TU_BREAKPOINT();
                return false;
        }
    }

    // Feature unit
    if(entityID == UAC2_ENTITY_FEATURE_UNIT) {
        switch(ctrlSel) {
            case AUDIO_FU_CTRL_MUTE:
                // Audio control mute cur parameter block consists of only one byte - we thus can send it right away
                // There does not exist a range parameter block for mute
                DEBUG("    Get Mute of channel: %u\r\n", channelNum);
                return tud_control_xfer(rhport, p_request, &mute[channelNum], 1);

            case AUDIO_FU_CTRL_VOLUME:
                switch(p_request->bRequest) {
                    case AUDIO_CS_REQ_CUR:
                        DEBUG("    Get Volume of channel: %u\r\n", channelNum);
                        return tud_control_xfer(rhport, p_request, &volume[channelNum], sizeof(volume[channelNum]));

                    case AUDIO_CS_REQ_RANGE:

                        // Copy values - only for testing - better is version below
                        audio_control_range_2_n_t(1) ret;

                        ret.wNumSubRanges = tu_htole16(1),
                        ret.subrange[0].bMin = tu_htole16(-VOLUME_CTRL_50_DB);
                        ret.subrange[0].bMax = tu_htole16(VOLUME_CTRL_0_DB);
                        ret.subrange[0].bRes = tu_htole16(256);

                        DEBUG("    Get channel %u volume range (%d, %d, %u) dB\r\n", channelNum,
                          ret.subrange[0].bMin / 256, ret.subrange[0].bMax / 256, ret.subrange[0].bRes / 256);

                        return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, (void*)&ret, sizeof(ret));

                        // Unknown/Unsupported control
                    default:
                        TU_BREAKPOINT();
                        return false;
                }
                break;

                // Unknown/Unsupported control
            default:
                TU_BREAKPOINT();
                return false;
        }
    }

    // Clock Source unit
    if(entityID == UAC2_ENTITY_CLOCK) {
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

                        // Unknown/Unsupported control
                    default:
                        TU_BREAKPOINT();
                        return false;
                }
                break;

            case AUDIO_CS_CTRL_CLK_VALID:
                // Only cur attribute exists for this request
                DEBUG("    Get Sample Freq. valid\r\n");
                return tud_control_xfer(rhport, p_request, &clkValid, sizeof(clkValid));

            // Unknown/Unsupported control
            default:
                TU_BREAKPOINT();
                return false;
        }
    }

    DEBUG("  Unsupported entity: %d\r\n", entityID);
    return false; // Yet not implemented
}


bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id, uint8_t ep_out, uint8_t cur_alt_setting) {
    (void)rhport;
    (void)func_id;
    (void)ep_out;
    (void)cur_alt_setting;

    uint16_t read;
    if(((n_bytes_received / 2) + spk_bufIn) > SPEAKER_BUFF_AND) {
        read = tud_audio_read(spk_buf + spk_bufIn, 2 * (SPEAKER_BUFF_SIZE - spk_bufIn));
        n_bytes_received -= read;
        spk_bufIn = 0;
        // DEBUG("read-p %d  ", read);
    }
    read = tud_audio_read(spk_buf + spk_bufIn, n_bytes_received);

    spk_bufIn += read / 2;
    spk_bufIn &= SPEAKER_BUFF_AND;

    // DEBUG("read %d\n", read);

    return true;
}

bool tud_audio_set_itf_close_EP_cb(uint8_t rhport, tusb_control_request_t const* p_request) {
    (void)rhport;
    (void)p_request;

    return true;
}

void tud_audio_feedback_params_cb(uint8_t func_id, uint8_t alt_itf, audio_feedback_params_t* feedback_param) {
    (void)func_id;
    (void)alt_itf;
    // Set feedback method to fifo counting
    feedback_param->method = AUDIO_FEEDBACK_METHOD_FIFO_COUNT;
    feedback_param->sample_freq = sampFreq;
}

void audioPwmWrap(void) {
    int16_t toPlay;
    // gets called every 48000Hz for a nice audio sampling rate
    if(pwm_get_irq_status_mask() & (1 << PWM_AUDIO_INT_SLICE)) {
        if(((spk_bufIn - spk_bufOut) & SPEAKER_BUFF_AND) != 0) {
            // DEBUG("%d-%d\n", spk_bufIn, spk_bufOut);
            // if(spk_bufOut != spk_bufIn){
            toPlay = spk_buf[spk_bufOut];

            spk_bufOut++;
            spk_bufOut &= SPEAKER_BUFF_AND;

            toPlay >>= 5;
            toPlay += 512;

            pwm_set_chan_level(PWM_OUTPUT_SLICE, PWM_CHAN_B, toPlay);
        }
        gpio_put(DEBUG_PIN, !gpio_get(DEBUG_PIN));
        pwm_clear_irq(PWM_AUDIO_INT_SLICE);
    }
}
