
- PINS: 0..=28 GPIO, of which 26..=28 can be analog
  - 1 analog pin (`mic_adc`)
  - 1 digital pin (neck `servo`)
  - 3 digital pins (`i2s_dac`)

- DMA: 12 available
  - 1 channel (`mic_adc` adc -> buffer)
  - 2 channels (`i2s_dac` bufferA -> PIO, bufferB -> PIO)

- DMA Interrupts: 2 available
  - 0: (`i2s_dac`: on buffer empty)

- PWM: 8 available, 2 channels per
  - 1 slice (neck `servo`)

- PIO Blocks: 2 blocks * 4 state machines
  - PIO0
    - 1 `sm` state machine (`i2s_dac`)

