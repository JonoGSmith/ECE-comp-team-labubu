# Note
Dev features of TinyUSB need to be used to fix a critical bug destroying CDC + UAC2.
At time of writing, the change hasn't been released in TUSB 0.19, and such the pico-sdk doesn't include it.

I've manually patched the changes into my version of the file `dcd_rp2040_fix.c`

https://github.com/hathach/tinyusb/pull/2937,

Thanks to the champion SomeRandomName99 for confirming this (https://github.com/raspberrypi/pico-sdk/issues/2236)