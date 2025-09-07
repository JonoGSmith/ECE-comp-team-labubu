#pragma once
#include "../common.hpp"
#include "i2s_protocol.hpp"
#include <hardware/dma.h>

// NOTE: The following is all explicitly temporary and is designed to play a file off the device itself.
// -----------------------------

INCBIN(gTestAudio, "res/mono48k-s16le-test-audio.raw");
inline u32 sPlayHeadSamplePosition = 0;
using MonoS16le = s16;

namespace dev::dac{
    static inline volatile u16 isDMA = 0;

    // NOTE: This is explicitly temporary.
    inline void load_samples(I2SOutBufHalf& into){
        auto audioData = (MonoS16le*)gTestAudioData;
        const auto audioDataLength = gTestAudioSize / sizeof(MonoS16le);
        for(auto& d: into){ // Copy and convert (s16 mono -> s32 stereo)
            d = I2SAudioSample::make(audioData[sPlayHeadSamplePosition++]);
        }
        sPlayHeadSamplePosition %= audioDataLength;
    }

    inline void dma_handler(){
        isDMA += 1; // TODO: debug counter
        bool bufA_empty = dma_channel_get_irq0_status(gDMADataA);
        auto& src_dma = bufA_empty ? gDMADataA : gDMADataB;
        auto& src_ptr = bufA_empty ? gI2SOutBufA : gI2SOutBufB;

        load_samples(src_ptr);

        // Prime the DMA that just finished to run again
        dma_channel_set_read_addr(src_dma, src_ptr.begin(), true);
        dma_channel_acknowledge_irq0(src_dma);
    }

}

