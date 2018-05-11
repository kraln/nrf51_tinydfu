/* Linker script to configure memory regions. */
MEMORY
{
  /* We're using the s130 soft device and we are the bootloader. */
  FLASH (rx) : ORIGIN = 0x0003AC00, LENGTH = 0x43FF
  STORAGE_REGION (r) : ORIGIN = 0x3F000, LENGTH = 0x400
  RAM (rwx) : ORIGIN = 0x20002000, LENGTH = 0x5FFF

  /** Location in UICR where bootloader start address is stored. */
  UICR_BOOTLOADER (r) : ORIGIN = 0x10001014, LENGTH = 0x04
}

SECTIONS
{
  /* Ensures the Bootloader start address in flash is written to UICR when flashing the image. */
  .uicrBootStartAddress :
  {
    KEEP(*(.uicrBootStartAddress))
  } > UICR_BOOTLOADER
}

SECTIONS
{
  . = ALIGN(4);
  .svc_data :
  {
    PROVIDE(__start_svc_data = .);
    KEEP(*(.svc_data))
    PROVIDE(__stop_svc_data = .);
  } > RAM
} INSERT AFTER .data

INCLUDE "gcc_nrf51_common.ld"
