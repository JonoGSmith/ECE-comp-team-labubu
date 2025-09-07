#pragma once
#include "../common.hpp"
#include "../system.hpp"

extern "C" {
    #include "i2s.pio.h"
}
#include <hardware/dma.h>
#include <hardware/pio.h>

// This file is derived in part from:
// micropython/ports/rp2/machine_i2s.c

namespace dev::dac{
    // Sample as seen by the DAC
    union I2SAudioSample{
        u64 raw;
        s32 c[2];
        struct{
            s32 l;
            s32 r;
        };

        static constexpr auto make(s16 s){
            return I2SAudioSample{.c = {s, s}};
        }
    };

    constexpr u32 cI2SSampleRate = 48'000;
    constexpr u8  cI2S_GPIO_DOUT = 3;
    constexpr u8  cI2S_GPIO_BCK = 4;
    constexpr u8  cI2S_GPIO_LCK = 5;
    static_assert(cI2S_GPIO_BCK + 1 == cI2S_GPIO_LCK, "Due to the PIO implementation, the BCK and LCK pins must be next to eachother.");

    constexpr u8  cI2SBitDepth   = 8 * sizeof(I2SAudioSample::c[0]);
    constexpr u32 cI2S_BCK_RATE  = cI2SSampleRate * cI2SBitDepth * 2;
    constexpr u32 cI2S_DOUT_RATE = cI2S_BCK_RATE;
    constexpr u32 cI2S_LCK_RATE  = cI2SSampleRate; // 50% low (L), 50% hi (R)
    static_assert(cI2SBitDepth == 32, "Only 32 bit output is supported for the project.");

    using I2SOutBufHalf = array<I2SAudioSample, (size_t)(cI2SSampleRate * 0.001)>; // This is 1ms each.
    inline I2SOutBufHalf gI2SOutBufA;
    inline I2SOutBufHalf gI2SOutBufB;

    inline DMAChannel gDMADataA;
    inline DMAChannel gDMADataB;
    // ----------------------------

    inline u8 init_pio(PIO pio){
        auto sm = pio_claim_unused_sm(pio, true);
        auto startAddr = pio_add_program(pio, &i2s_data_write_program);

        // PIO Block
        pio_sm_config sm_config = i2s_data_write_program_get_default_config(startAddr);
        sm_config_set_sideset_pins(&sm_config, cI2S_GPIO_BCK); // The BCK and LRCK pins must be next to eachother. They need no data.
        sm_config_set_out_pins(&sm_config, cI2S_GPIO_DOUT, 1);
        sm_config_set_out_shift(&sm_config, false, false, 32); // shift register is 4 bytes
        sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX); // fifo to the shift register is 8 bytes (this is the memory location we dma write to)
        pio_sm_init(pio, sm, startAddr, &sm_config);

        constexpr f32 target_rate = cI2S_BCK_RATE * 2;
        constexpr f32 clock_div = sys::cClockRate / target_rate;
        pio_sm_set_clkdiv(pio, sm, clock_div);

        // Pins
        pio_gpio_init(pio, cI2S_GPIO_DOUT);
        pio_gpio_init(pio, cI2S_GPIO_BCK);
        pio_gpio_init(pio, cI2S_GPIO_LCK + 1);
        constexpr u32 pin_mask = (1 << cI2S_GPIO_DOUT) | (1 << cI2S_GPIO_BCK) | (1 << cI2S_GPIO_LCK);
        pio_sm_set_pins_with_mask(pio, sm, 0, pin_mask);  // zero output to start with
        pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask); // all outputs (1)

        return sm;
    }

    inline void dma_handler();
    inline void init_dma(PIO pio, u8 sm){
        gDMADataA = dma_claim_unused_channel(true);
        gDMADataB = dma_claim_unused_channel(true);

        // The DMA channels are chained together.
        // The first DMA channel is used to access the top half of the DMA buffer.
        // The second DMA channel accesses the bottom half of the DMA buffer.
        // With chaining, when one DMA channel has completed a data transfer, the other DMA channel automatically starts a new data transfer.
        auto configure = [&](DMAChannel ch, I2SOutBufHalf& buffer){
            auto cfg = dma_channel_get_default_config(ch);
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32); // matches the tx shift register
            channel_config_set_chain_to(&cfg, ch == gDMADataA ? gDMADataB : gDMADataA); // the key
            channel_config_set_read_increment(&cfg, true);   // reading from the buffer
            channel_config_set_write_increment(&cfg, false); // writing to the PIO block
            channel_config_set_dreq(&cfg, pio_get_dreq(pio, sm, true)); // this ensures the dma doesn't overflow the PIO
            dma_channel_configure(ch, &cfg, &pio->txf[sm], buffer.begin(), dma_encode_transfer_count(sizeof(buffer) / sizeof(u32)), false); // gI2SOutBuf.a.begin(), gI2SOutBuf.length,
        };
        configure(gDMADataA, gI2SOutBufA);
        configure(gDMADataB, gI2SOutBufB);

        auto prime_interrupts = [](DMAChannel ch){
            dma_channel_acknowledge_irq0(ch);
            dma_channel_set_irq0_enabled(ch, true);
        };
        prime_interrupts(gDMADataA);
        prime_interrupts(gDMADataB);
    }

    inline void init(){
        auto const& pio = pio0; // chosen pio
        auto sm = init_pio(pio);
        init_dma(pio, sm); // set up dma to feed the state machine
        pio_sm_set_enabled(pio, sm, true); // Start the pio block. Empty I2S should be produced.

        // Enable the interrupts
        irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
        irq_set_enabled(DMA_IRQ_0, true);
    }

    inline void start(){
        dma_channel_start(gDMADataA); // Start the alternating DMAs. I2S sound should be produced.
    }

}