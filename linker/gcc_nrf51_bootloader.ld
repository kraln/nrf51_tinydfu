/* Linker script to configure memory regions. */
MEMORY
{
  /* We're using the s130 soft device and we are the bootloader. */
  FLASH (rx) : ORIGIN = 0x0003AC00, LENGTH = 0x43FF
  STORAGE_REGION (r) : ORIGIN = 0x3F000, LENGTH = 0x400
  RAM (rwx) : ORIGIN = 0x20002000, LENGTH = 0x5FFF
}
INCLUDE "gcc_nrf51_common.ld"
