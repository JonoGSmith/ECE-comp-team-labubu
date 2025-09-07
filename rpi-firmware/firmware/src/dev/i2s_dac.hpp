#pragma once
#include "../common.hpp"
#include "i2s_protocol.hpp"
#include "../ring_array.hpp"
#include "../binaries.hpp"

#include <hardware/dma.h>

// NOTE: The following is all explicitly temporary and is designed to play a file off the device itself.
// -----------------------------


inline u32 sPlayHeadSamplePosition = 0;

namespace dev::dac{
    static inline volatile u16 isDMA = 0;

    using MonoAudioSample = s16;
    static inline RingArray<MonoAudioSample, (1 << 11)> gAudioOutputBuffer;

    // NOTE: This is explicitly temporary.
    inline void load_samples(I2SOutBufHalf& into){
        // TODO: More efficient copy
        auto audioData = (s8*)&gTestAudioData[0];
        const auto audioDataLength = gTestAudioSize;
        for(auto& d: into){ // Copy and convert (s16 mono -> s32 stereo)
            d = I2SAudioSample{.l = (s32)audioData[sPlayHeadSamplePosition] << 19, .r = (s32)audioData[sPlayHeadSamplePosition] << 19};
            sPlayHeadSamplePosition += 1;
            sPlayHeadSamplePosition %= audioDataLength;
        }
    }

    inline void dma_handle_channel(DMAChannel ch, I2SOutBufHalf& buf){
        bool needs_servicing = dma_channel_get_irq0_status(ch);
        if(!needs_servicing){ return; }

        load_samples(buf);

        // Prime the DMA that just finished to run again
        dma_channel_set_read_addr(ch, buf.begin(), false);
        dma_channel_acknowledge_irq0(ch);
    }

    inline void dma_handler(){
        isDMA += 1; // TODO: debug counter
        dma_handle_channel(gDMADataA, gI2SOutBufA);
        dma_handle_channel(gDMADataB, gI2SOutBufB);
    }

}

