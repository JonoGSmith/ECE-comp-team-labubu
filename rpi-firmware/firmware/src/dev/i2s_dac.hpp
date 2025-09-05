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

    inline void dma_handler(){
        isDMA += 1;

        auto audioData = (MonoS16le*)gTestAudioData;
        const auto audioDataLength = gTestAudioSize / sizeof(MonoS16le);

        auto dst = (void*)dma_channel_hw_addr(gDMADataOut)->read_addr == gI2SOutBuf.b.begin()
                 ? gI2SOutBuf.a.begin() : gI2SOutBuf.b.begin();
        for(size_t i = 0; i < gI2SOutBuf.length; i++){ // Copy and make stereo
            *dst = I2SAudioSample::make(audioData[sPlayHeadSamplePosition]);
            dst++;
        }
        sPlayHeadSamplePosition += gI2SOutBuf.length;
        sPlayHeadSamplePosition %= audioDataLength;

        // TODO: Temporary until the double-dma is sorted out. Need to confirm samples are actually produced first.
        auto dst2 = dst == gI2SOutBuf.a.begin() ? gI2SOutBuf.b.begin() : gI2SOutBuf.a.begin();
        dma_channel_set_read_addr(gDMADataOut, dst2, true);
        dma_channel_acknowledge_irq0(gDMADataOut);
    }

    inline void start(){
        dma_channel_set_read_addr(gDMADataOut, gI2SOutBuf.a.begin(), false);
        dma_channel_start(gDMADataOut);
    }
}

