#pragma once
#include "../common.hpp"
extern "C" {
    #include "i2s.pio.h"
    #include "i2s/i2s.h"
}
#include "hardware/dma.h"

namespace dev::dac{
    namespace cfg{
        constexpr auto i2s_cfg = i2s_config{
            .fs             = 48'000,
            .sck_mult       = 0, // unused
            .bit_depth      = 16,
            .sck_pin        = 0, // unused
            .dout_pin       = 3, // GPIO pin numbers
            .din_pin        = 4,
            .clock_pin_base = 5,
            .sck_enable     = false
        };
    }
    static inline pio_i2s gI2Spio alignas(8);

    inline void dma_handler();
    inline void init(){
        using namespace cfg;

        i2s_program_start_synched(pio0, &i2s_cfg, &dma_handler, &gI2Spio);
    }

}
// NOTE: The following is all explicitly temporary and is designed to play a file off the device itself.
// -----------------------------
#include "incbin.h"
INCBIN(gTestAudio, "res/mono48k-s16le-test-audio2.raw");
inline u32 sPlayHeadSamplePosition = 0;

namespace dev::dac{
    inline void process_audio(const int32_t* input, int32_t* output, size_t num_frames) {
        // Just copy the input to the output
        for (size_t i = 0; i < num_frames * 2; i++) {
            output[i] = input[i];
        }
    }
    inline void dma_handler(){
        auto& i2s = gI2Spio;
        /* We're double buffering using chained TCBs. By checking which buffer the
        * DMA is currently reading from, we can identify which buffer it has just
        * finished reading (the completion of which has triggered this interrupt).
        */
        if (*(int32_t**)dma_hw->ch[i2s.dma_ch_in_ctrl].read_addr == i2s.input_buffer) {
            // It is inputting to the second buffer so we can overwrite the first
            process_audio(i2s.input_buffer, i2s.output_buffer, AUDIO_BUFFER_FRAMES);
        } else {
            // It is currently inputting the first buffer, so we write to the second
            process_audio(&i2s.input_buffer[STEREO_BUFFER_SIZE], &i2s.output_buffer[STEREO_BUFFER_SIZE], AUDIO_BUFFER_FRAMES);
        }

        dma_channel_acknowledge_irq0(i2s.dma_ch_in_data);
    }
}

