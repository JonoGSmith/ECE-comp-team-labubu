
- PINS: 0..=28 GPIO, of which 26..=28 can be analog
  - 1 analog pin (`mic_adc`)
  - 1 digital pin (neck `servo`)

- DMA: 12 available
  - 1 channel (`mic_adc` adc -> buffer)
  - 1 channel (`i2s_dac` buffer -> pio)

- DMA Interrupts: 2 available
  - 0: (`i2s_dac`: on buffer empty)

- PWM: 8 available, 2 channels per
  - 1 slice (neck `servo`)

- PIO Blocks: 2 blocks * 4 state machines
  - PIO0
    - 1 `sm` state machine (`i2s_dac`)
