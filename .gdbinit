file build/nrf51_tinydfu.elf
target remote localhost:2331
mon semihosting enable
mon semihosting ThumbSWI 0xAB

#break main
