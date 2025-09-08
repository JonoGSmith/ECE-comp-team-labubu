
- PINS: 0..=28 GPIO, of which 26..=28 can be analog
  - 1 analog pin (`mic_adc`)
  - 1 digital pin (neck `servo` pwm)
  - 3 digital pins (`i2s_dac`)

- DMA: 12 available
  - 2 channel (`mic_adc` adc -> bufferA, adc -> bufferB)
  - 2 channels (`i2s_dac` bufferA -> PIO, bufferB -> PIO)

- DMA Interrupts: 2 available
  - 0: (`i2s_dac`: on buffer empty)
  - 1: (`mic_adc`: on buffer full)
  - NOTE: It is possible to overload these interrupts with more functions

- PWM: 8 available, 2 channels per
  - 1 slice (neck `servo`)

- PIO Blocks: 2 blocks * 4 state machines
  - PIO0
    - 1 `sm` state machine (`i2s_dac`)
