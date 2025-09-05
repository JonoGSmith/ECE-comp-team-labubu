#pragma once
#include "../common.hpp"
#include "../system.hpp"

extern "C" {
    #include "i2s.pio.h"
}
#include <hardware/dma.h>
#include <hardware/pio.h>

// NOTES About how the audio feeding mechanism works.
// - There's a custom PIO block programmed to control the 3 pins required for I2S audio transmission.
// - The data is fed into an 8-byte fifo (PIO_FIFO_JOIN_TX) using DMA transfers.
// - In order to create seemless playback, the audio buffer needs to be constantly stocked with new samples, and seemlessly wrap around once it reaches the end.
// - To do this, a two-stage DMA solution is employed
//      - Two half buffers exist in ram.
//      - A array<buffer*, 2> exists with the addresses of the two buffers in it.
//      - One DMA copies 'buffer -> fifo', and when its done, it triggers the second DMA. It runs forever.
//      - This DMA replaces the buffer pointer used by the first one to the next buffer. It is a ring, and runs forever. It also triggers an interrupt to allow us to restock the inactive buffer.
// - These sample buffers are 32bps, 48kHz, stereo (a format supported by the I2S DAC) - this requires a conversion step from the source 48kHz mono 16bps as it is being populated


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

    constexpr u8  cI2SBitDepth   = 8 * sizeof(I2SAudioSample::c[0]);
    constexpr u32 cI2S_BCK_RATE  = cI2SSampleRate * cI2SBitDepth * 2;
    constexpr u32 cI2S_DOUT_RATE = cI2S_BCK_RATE;
    constexpr u32 cI2S_LCK_RATE  = cI2SSampleRate; // 50% low (L), 50% hi (R)

    struct I2SOutputBuffers{
        static constexpr size_t length = cI2SSampleRate * 0.001; // This is 1ms each.
        array<I2SAudioSample, length> a;
        array<I2SAudioSample, length> b;
    };
    // The 8 bytes alignment requirement is important else the second-layer DMA doesn't correctly form a ring.
    struct alignas(8) I2SOutputPointers{
        array<I2SAudioSample*, 2> ptrs;
    };

    inline I2SOutputBuffers gI2SOutBuf;
    constexpr I2SOutputPointers cI2SOutBufPtrs = {gI2SOutBuf.a.begin(), gI2SOutBuf.b.begin()};

    inline u8 gDMADataOut;
    // inline u8 gDMADoubleBufferCtrl;
    // ----------------------------

    inline u32 num_transfers(){
        return dma_encode_transfer_count(sizeof(gI2SOutBuf.a) / sizeof(u32));
    }


    inline tup<u8, u8> init_pioblock_i2s_data_write(PIO pio){
        constexpr auto target_rate = cI2S_BCK_RATE * 2; // NOTE: The target rate is not cleanly a 4 bit fractional component. May require a master clock adjustment.
        constexpr f32 clock_div = (f32)sys::cClockRate / target_rate;

        auto sm = pio_claim_unused_sm(pio, true);
        u8 sm_mask = (1u << sm);
        auto startAddr = pio_add_program(pio, &i2s_data_write_program);

        [&](u8 pin_DOUT, u8 pin_pair_first){
            pio_gpio_init(pio, pin_DOUT);
            pio_gpio_init(pio, pin_pair_first);
            pio_gpio_init(pio, pin_pair_first + 1);

            pio_sm_config sm_config = i2s_data_write_program_get_default_config(startAddr);
            sm_config_set_sideset_pins(&sm_config, pin_pair_first); // The BCK and LRCK pins must be next to eachother. They need no data.
            sm_config_set_out_pins(&sm_config, pin_DOUT, 1);
            sm_config_set_fifo_join(&sm_config, PIO_FIFO_JOIN_TX); // fifo to the shift register is 8 bytes (this is the memory location we dma write to)
            sm_config_set_out_shift(&sm_config, false, false, 32); // shift register is 4 bytes
            pio_sm_init(pio, sm, startAddr, &sm_config);

            uint32_t pin_mask = (0b1u << pin_DOUT) | (0b11u << pin_pair_first);
            pio_sm_set_pins_with_mask(pio, sm, 0, pin_mask);  // zero output to start with
            pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask); // all outputs (1)
        }(cI2S_GPIO_DOUT, cI2S_GPIO_BCK);

        pio_sm_set_clkdiv(pio, sm, clock_div);
        return {sm, sm_mask};
    }

    inline void dma_handler();
    inline void init_dma(PIO pio, u8 sm){
        gDMADataOut = dma_claim_unused_channel(true);

        // This DMA channel feeds the 8 byte fifo buffer to the PIO block.
        // The PIO block then takes 4 bytes into a shift register, and writes that on DOUT.
        auto cfg = dma_channel_get_default_config(gDMADataOut);
        channel_config_set_read_increment(&cfg, true);   // reading from the buffer
        channel_config_set_write_increment(&cfg, false); // writing to the PIO block
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32); // matches the tx shift register
        channel_config_set_dreq(&cfg, pio_get_dreq(pio, sm, true)); // this ensures the dma doesn't overflow the buffer
        dma_channel_configure(gDMADataOut, &cfg, &pio->txf[sm], NULL, num_transfers(), false); // gI2SOutBuf.a.begin(), gI2SOutBuf.length,
        // TODO: Second stage dma
        // TODO: First stage dma resume

        // Temporary irq
        dma_channel_set_irq0_enabled(gDMADataOut, true);
        irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
        irq_set_enabled(DMA_IRQ_0, true);
    }

    inline void init(){
        auto const& pio = pio0; // chosen pio
        auto [sm, sm_mask] = init_pioblock_i2s_data_write(pio); // upload state machine to pio block
        init_dma(pio, sm); // set up dma to feed the state machine
        pio_enable_sm_mask_in_sync(pio, sm_mask); // start the pio block
    }

}