#pragma once
#include "../common.hpp"
#include "i2s_protocol.hpp"

#include <hardware/dma.h>

namespace dev::dac{
    static inline volatile u16 isDMA = 0;

    // TODO: Volume control? Non-essential

    inline void load_samples(I2SOutBufHalf& into){
        // The buffer needs to be completely filled with samples.
        // We take as much as we can from gAudioRecvBuffer till it's empty, then we spit out zeros
        auto recvCurrLength = gAudioRecvBuffer.length();
        size_t w = 0;
        while(w < into.size() && w < recvCurrLength){
            auto word = gAudioRecvBuffer.read_one();
            // s16 sword = word;
            s32 sample = word;
            into[w] = I2SAudioSample{.l = sample * (1 << 11), .r = sample * (1 << 11)}; // quieted for my ears' sanity.
            w += 1;
        }
        // Run out of audio. This supresses garbage but indicates not enough data.
        while(w < into.size()){
            into[w] = I2SAudioSample{.l = 0, .r = 0};
            w += 1;
        }
    }

    inline void dma_handle_channel(DMAChannel ch, I2SOutBufHalf& buf){
        bool needs_servicing = dma_channel_get_irq0_status(ch);
        if(!needs_servicing){ return; }

        load_samples(buf);

        // Prime the DMA that finished. It'll be auto-triggered by the other one when ready.
        dma_channel_set_read_addr(ch, buf.begin(), false);
        dma_channel_acknowledge_irq0(ch);
    }

    inline void dma_handler(){
        isDMA += 1; // Debug counter
        dma_handle_channel(gDMADataA, gI2SOutBufA);
        dma_handle_channel(gDMADataB, gI2SOutBufB);
    }

}

