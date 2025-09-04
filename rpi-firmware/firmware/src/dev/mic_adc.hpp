#pragma once
#include "../common.hpp"
#include "../system.hpp"
#include <hardware/adc.h>
#include <hardware/dma.h>

// For reading from a mono-channel microphone.
// Uses DMA to collect samples, then processes in chunks once ready.
// -------------------------------------------

namespace dev::mic{
    namespace cfg{
        constexpr u32 ADC_PIN = 2;
        constexpr u32 SAMPLE_RATE = 20'000;
    }

    inline array<u16, 1024> gSampleBuffer;
    inline u8 gDMAChannel;

    inline void init(){
        using namespace cfg;
        // Arm the ADC
        adc_init();
        adc_gpio_init(ADC_PIN + 26); // Select the pin of choice
        adc_select_input(ADC_PIN);
        adc_fifo_setup(
            true, // on
            true, // use dma
            1,    // DMA after N samples present - the hardware fifo is 4 or 8 samples long (should it be =4?)
            false,// ignore errors - this is audio
            false // false = 16 bit, true = 8 bit
        );

        adc_set_clkdiv((float)sys::cClockRate/SAMPLE_RATE);
        // adc_set_temp_sensor_enabled(false); // hmm

        // Arm the DMA
        gDMAChannel = dma_claim_unused_channel(true);
        auto cfg = dma_channel_get_default_config(gDMAChannel);
        channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
        channel_config_set_read_increment(&cfg, false); // the fifo is in a fixed location
        channel_config_set_write_increment(&cfg, true); // write into the ring buffer
        channel_config_set_dreq(&cfg, DREQ_ADC); // the dma triggers based on the adc
        dma_channel_configure(gDMAChannel, &cfg, NULL, &adc_hw->fifo, gSampleBuffer.size(), false);
    }
    inline void start(){
        dma_channel_set_write_addr(gDMAChannel, gSampleBuffer.begin(), true);
        adc_run(true);
    }
}
