#pragma once
#include "../common.hpp"
#include "../system.hpp"
#include <hardware/adc.h>
#include <hardware/dma.h>
#include <tusb.h>

// For reading from a mono-channel microphone.
// Uses 2 DMAs in a ring formation to collect samples (same as i2s),
// then processes the samples in chunks when the usb requests the data.
// Uses DMA IRQ 1
// -------------------------------------------

namespace dev::mic{
    namespace cfg{
        constexpr u32 ADC_PIN = 2;
        constexpr u32 SAMPLE_RATE = 48'000; // ehhh - rather be slower but its ok.
        constexpr f64 ADC_LEVEL_SHIFT = 1.5; // Volts
        // These are helper constants
        constexpr u32 ADC_PRECISION = 12; // bit depth
        constexpr f64 ADC_VREF = 3.3;
        constexpr f64 ADC_DELTA = ADC_VREF / ((1 << ADC_PRECISION) - 1);
        constexpr u16 ADC_LEVEL_SHIFT_COUNT = (ADC_LEVEL_SHIFT / ADC_DELTA) * (1 << (16 - ADC_PRECISION));
    }

    using USBAudioSample16 = s16;
    using ADCAudioSampleRaw = u16; // Level shifted 12 bit adc output
    using ADCInBufHalf = array<ADCAudioSampleRaw, (size_t)(cfg::SAMPLE_RATE * 0.001)>; // 5ms
    inline DMAChannel gDMAadcA;
    inline DMAChannel gDMAadcB;
    inline ADCInBufHalf gSampleBufferA;
    inline ADCInBufHalf gSampleBufferB;
    inline bool gSampleBufferAFull = false;
    inline bool gSampleBufferBFull = false;

    inline u32 gDMACount = 0;

    inline void adc_dma_handler();
    inline void init(){
        using namespace cfg;
        // Arm the ADC
        adc_init();
        adc_gpio_init(ADC_PIN + 26); // Select the pin of choice
        adc_select_input(ADC_PIN);
        adc_fifo_setup(true,
            true, // use dma
            1,    // DMA after N samples present - the hardware fifo is 4 samples long
            false,// ignore errors - this is audio we have to try recover
            false // false = 16 bit, true = 8 bit
        );

        constexpr auto clock_div = (f32)48'000'000.0/cfg::SAMPLE_RATE; // the ADC runs on its own 48MHz clock independently
        adc_set_clkdiv(clock_div);
        // adc_set_temp_sensor_enabled(false); // hmm

        // Arm the DMAs (alternating)
        gDMAadcA = dma_claim_unused_channel(true);
        gDMAadcB = dma_claim_unused_channel(true);
        auto configure = [&](DMAChannel ch, ADCInBufHalf& buffer){
            auto cfg = dma_channel_get_default_config(ch);
            channel_config_set_chain_to(&cfg, ch == gDMAadcA ? gDMAadcB : gDMAadcA); // the key
            channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
            channel_config_set_read_increment(&cfg, false); // the fifo is in a fixed location
            channel_config_set_write_increment(&cfg, true); // write into the ring buffer
            channel_config_set_dreq(&cfg, DREQ_ADC);        // the dma triggers based on the adc
            dma_channel_configure(ch, &cfg, buffer.begin(), &adc_hw->fifo, sizeof(buffer) / sizeof(u16), false);
        };
        configure(gDMAadcA, gSampleBufferA);
        configure(gDMAadcB, gSampleBufferB);

        // Interrupts
        auto prime_interrupts = [](DMAChannel ch){
            dma_channel_acknowledge_irq1(ch);
            dma_channel_set_irq1_enabled(ch, true);
        };
        prime_interrupts(gDMAadcA);
        prime_interrupts(gDMAadcB);

        irq_set_exclusive_handler(DMA_IRQ_1, adc_dma_handler);
        irq_set_enabled(DMA_IRQ_1, true);

    }
    inline void start(){
        adc_run(true);
        dma_channel_start(gDMAadcA); // start the ping-pong
    }

    // Add to the USB audio outgoing buffer
    inline void offload_samples(ADCInBufHalf const& from){
        auto bytesWritten = tud_audio_write((uint8_t*)from.begin(), sizeof(from));
    }

    inline void dma_handle_channel(DMAChannel ch, ADCInBufHalf& buf, bool& full){
        bool needs_servicing = dma_channel_get_irq1_status(ch);
        if(!needs_servicing){ return; }

        offload_samples(buf);
        full = true;

        // Prime the DMA that finished. It'll be auto-triggered by the other one when ready.
        dma_channel_set_write_addr(ch, buf.begin(), false);
        dma_channel_acknowledge_irq1(ch);
    }

    inline void adc_dma_handler(){
        gDMACount += 1; // Debug counter
        dma_handle_channel(gDMAadcA, gSampleBufferA, gSampleBufferAFull);
        dma_handle_channel(gDMAadcB, gSampleBufferB, gSampleBufferBFull);
    }
}
