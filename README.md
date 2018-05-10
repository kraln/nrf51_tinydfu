Tiny DFU for nRF51
==================

Honestly the Nordic DFU is a mess, and difficult to comprehend. I just need something simple to understand and debug which lets me, potentially, update a firmware on an nRF51-based device.

I locate myself at 0x0003AC00, so you need to tell the nRF51 SOC that there is a bootloader there.
nrfjprog.exe --memwr 0x10001014 --val 0x0003AC00


