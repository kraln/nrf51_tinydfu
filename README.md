# Tiny DFU for nRF51

A simple-as-possible bootloader with DFU (own protocol over "Nordic" serial). 

## Why?

For me the examples that ship with the Nordic SDK are really quite difficult to tease apart. They contain lots of "extra" functionality, and particularly the Nordic DFU is difficult for me to comprehend. I just need something simple to understand and debug which lets me, potentially, update a firmware on an nRF51-based device.

## What do I need to know?

My toolchain is arm-none-eabi-gcc, a relatively recent one (6.3 at least). ccache, ctags, etc. are also expected.

This project uses the Softdevice S110 from the SDK V10, which you should be able to get from here: https://developer.nordicsemi.com/nRF5_SDK/nRF51_SDK_v10.x.x/ 
You will need to edit the Makefile to locate the Nordic SDK

The bootloader locates itself at 0x0003C000, so you need to tell the nRF51 SOC that there is a bootloader there.
nrfjprog.exe --memwr 0x10001014 --val 0x0003C000

The linkerscript *should* do this when you flash the hex file. 

Currently, this project is built for the (non-standard) case of a 32MHz Crystal. If you want to use it with a 16MHz Crystal, simply remove the 32MHz function in main, and change the define in system_nrf51.c. 
