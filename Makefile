# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0/

##########################################################################
# User configuration and firmware specific object files
##########################################################################
NAME = nrf51_tinydfu
TARGET := NRF51
SHELL = /bin/bash
SDK_ROOT = ../../../../mnt/d/nrfsdk12/nRF5_SDK_12.3.0_d7731ad

# Debugging stuff
TERMINAL ?= xterm -e
JLINKEXE := JLinkExe
JLINKGDB := JLinkGDBServer
JLINK_OPTIONS = -device nrf51822 -if swd -speed 1000

# Flash start address
FLASH_START_ADDRESS := 0x0003AC00

# Cross-compiler
TARGET_TRIPLE := arm-none-eabi

# Build Directory
BUILD := build

SOURCES := \
	source \
	linker 

INCLUDES := \
	source \
	linker \
	include \
	include/gcc 

# NRF Includes (prune me)
INCLUDES += \
  $(SDK_ROOT)/components/drivers_nrf/comp \
  $(SDK_ROOT)/components/ble/ble_services/ble_ancs_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_ias_c \
  $(SDK_ROOT)/components/softdevice/s130/headers \
  $(SDK_ROOT)/components/ble/ble_services/ble_gls \
  $(SDK_ROOT)/components/libraries/gpiote \
  $(SDK_ROOT)/components/drivers_nrf/gpiote \
  $(SDK_ROOT)/components/drivers_nrf/common \
  $(SDK_ROOT)/components/ble/ble_advertising \
  $(SDK_ROOT)/components/softdevice/s130/headers/nrf51 \
  $(SDK_ROOT)/components/ble/ble_services/ble_bas_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_hrs_c \
  $(SDK_ROOT)/components/ble/ble_dtm \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/components/ble/ble_services/ble_rscs_c \
  $(SDK_ROOT)/components/ble/common \
  $(SDK_ROOT)/components/ble/ble_services/ble_lls \
  $(SDK_ROOT)/components/drivers_nrf/wdt \
  $(SDK_ROOT)/components/ble/ble_services/ble_bas \
  $(SDK_ROOT)/components/libraries/experimental_section_vars \
  $(SDK_ROOT)/components/ble/ble_services/ble_ans_c \
  $(SDK_ROOT)/components/libraries/slip \
  $(SDK_ROOT)/components/libraries/mem_manager \
  $(SDK_ROOT)/components/libraries/csense_drv \
  $(SDK_ROOT)/components/drivers_nrf/hal \
  $(SDK_ROOT)/components/ble/ble_services/ble_nus_c \
  $(SDK_ROOT)/components/drivers_nrf/rtc \
  $(SDK_ROOT)/components/ble/ble_services/ble_ias \
  $(SDK_ROOT)/components/drivers_nrf/ppi \
  $(SDK_ROOT)/components/ble/ble_services/ble_dfu \
  $(SDK_ROOT)/components \
  $(SDK_ROOT)/components/libraries/scheduler \
  $(SDK_ROOT)/components/ble/ble_services/ble_lbs \
  $(SDK_ROOT)/components/ble/ble_services/ble_hts \
  $(SDK_ROOT)/components/drivers_nrf/delay \
  $(SDK_ROOT)/components/drivers_nrf/timer \
  $(SDK_ROOT)/components/libraries/util \
  $(SDK_ROOT)/components/libraries/usbd/class/cdc \
  $(SDK_ROOT)/components/ble/ble_services/ble_cscs \
  $(SDK_ROOT)/components/drivers_nrf/lpcomp \
  $(SDK_ROOT)/components/libraries/timer \
  $(SDK_ROOT)/components/drivers_nrf/power \
  $(SDK_ROOT)/components/libraries/usbd/config \
  $(SDK_ROOT)/components/toolchain \
  $(SDK_ROOT)/components/libraries/led_softblink \
  $(SDK_ROOT)/components/ble/ble_services/ble_cts_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_nus \
  $(SDK_ROOT)/components/ble/ble_services/ble_hids \
  $(SDK_ROOT)/components/ble/peer_manager \
  $(SDK_ROOT)/components/ble/ble_services/ble_tps \
  $(SDK_ROOT)/components/ble/ble_services/ble_dis \
  $(SDK_ROOT)/components/device \
  $(SDK_ROOT)/components/ble/nrf_ble_qwr \
  $(SDK_ROOT)/components/ble/ble_services/ble_lbs_c \
  $(SDK_ROOT)/components/ble/ble_racp \
  $(SDK_ROOT)/components/toolchain/gcc \
  $(SDK_ROOT)/components/ble/ble_services/ble_rscs \
  $(SDK_ROOT)/components/softdevice/common/softdevice_handler \
  $(SDK_ROOT)/components/softdevice/s130/headers 



LIBRARIES :=
LD_SCRIPT := gcc_nrf51_bootloader.ld

##########################################################################
# Compiler prefix and location
##########################################################################
CCACHE = ccache

TARGET_PREFIX = $(TARGET_TRIPLE)-
TARGET_AS = $(CCACHE) $(TARGET_PREFIX)gcc
TARGET_CC = $(CCACHE) $(TARGET_PREFIX)gcc 
TARGET_GDB = $(TARGET_PREFIX)gdb
TARGET_LD = $(CCACHE) $(TARGET_PREFIX)gcc
TARGET_SIZE = $(TARGET_PREFIX)size
TARGET_OBJCOPY = $(TARGET_PREFIX)objcopy
TARGET_OBJDUMP = $(TARGET_PREFIX)objdump

##########################################################################
# Misc. stuff
##########################################################################
UNAME := $(shell uname -s)

PROGRAM_VERSION := \
	$(shell git describe --always --tags --abbrev=7 2>/dev/null | \
	sed s/^v//)
ifeq ("$(PROGRAM_VERSION)","")
    PROGRAM_VERSION:='unknown'
endif

##########################################################################
# Compiler settings
##########################################################################

COMMON_FLAGS := -g -c -Wall -Werror -ffunction-sections -fdata-sections \
	-fmessage-length=0 -std=gnu99 \
	-DTARGET=$(TARGET) -D$(TARGET) \
	-DS130 -DBLE_STACK_SUPPORT_REQD -DNRF_SD_BLE_API_VERSION=2 -DSOFTDEVICE_PRESENT \
	-DPROGRAM_VERSION=\"$(PROGRAM_VERSION)\" \
#	-DSEMIHOSTED \

COMMON_ASFLAGS := -D__ASSEMBLY__ -x assembler-with-cpp

TARGET_ARCHFLAGS := -march=armv6-m -mthumb -mcpu=cortex-m0
TARGET_FLAGS := $(COMMON_FLAGS) -Os -g3 -fmerge-constants $(TARGET_ARCHFLAGS) $(INCLUDE)
TARGET_CFLAGS := $(TARGET_FLAGS)
TARGET_ASFLAGS := $(TARGET_FLAGS) $(COMMON_ASFLAGS)

TARGET_LDFLAGS = $(TARGET_ARCHFLAGS) -Wl,--gc-sections
TARGET_LDLIBS  =

##########################################################################
# Top-level Makefile
##########################################################################

ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUTDIR := $(CURDIR)
export OUTPUT := $(OUTPUTDIR)/$(NAME)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) 
export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CFILES += \
  $(SDK_ROOT)/components/ble/common/ble_conn_params.c \
	$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler.c \
  $(SDK_ROOT)/components/ble/ble_services/ble_nus/ble_nus.c

SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
HFILES := $(foreach dir,$(INCLUDES),$(notdir $(wildcard $(dir)/*.h)))
export LIBFILES := $(addprefix $(CURDIR)/,$(foreach dir,$(LIBRARIES),$(wildcard $(dir)/*.a)))

export OFILES := $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir))/ \
                  -I$(CURDIR)/$(BUILD)/

##########################################################################
# Targets
##########################################################################

.PHONY: $(BUILD) clean ctags test dotest flash debug

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(NAME)_test $(NAME)_test.exe $(NAME)_test.xml \
		$(OUTPUT).bin $(OUTPUT).hex tags test_coverage

ctags:
	@[ -d $(BUILD) ] || mkdir -p $(BUILD)
	@make --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile ctags

flash: $(BUILD)
	echo -e "r\nloadbin $(NAME).bin, $(FLASH_START_ADDRESS)\nr\ng\nexit\n" > .jlinkflash
	-$(JLINKEXE) $(JLINK_OPTIONS) .jlinkflash

debug: flash
	echo -e "file $(BUILD)/$(NAME).elf\n target remote localhost:2331\nbreak main\nmon semihosting enable\nmon semihosting ThumbSWI 0xAB\n" > .gdbinit
	$(TERMINAL) "$(JLINKGDB) $(JLINK_OPTIONS) -port 2331" &
	sleep 1
	$(TERMINAL) "telnet localhost 2333" &
	$(TERMINAL) "$(TARGET_GDB) $(BUILD)/$(NAME).elf"

##########################################################################
# Build-level Makefile
##########################################################################
else

DEPENDS := $(OFILES:.o=.d)

# Targets
.PHONY: ctags

all: $(OUTPUTDIR)/tags $(OUTPUT).bin $(OUTPUT).hex $(NAME).objdump 

ctags: $(OUTPUTDIR)/tags

##########################################################################
# Project-specific Build Rules
##########################################################################
$(OUTPUT).bin: $(NAME).bin
	@cp $< $@

$(OUTPUT).hex: $(NAME).hex
	@cp $< $@

$(NAME).elf: $(OFILES)
	@echo $(@F)
	@$(TARGET_LD) $(TARGET_LDFLAGS) -Wl,-Map=$(@F).map \
		-L $(CURDIR)/../linker -T $(LD_SCRIPT) -o $@ $(OFILES) \
		$(TARGET_LDLIBS) $(LIBFILES)

	-@( cd $(OUTPUTDIR); )

$(NAME).objdump: $(NAME).elf

$(OUTPUTDIR)/tags: $(CFILES) $(HFILES)
	@ctags -f $@ --totals -R \
		$(OUTPUTDIR)/source \
		$(OUTPUTDIR)/include \
		$(OUTPUTDIR)/Makefile

###
### Softdevice Uglyness
###
$(SDK_ROOT)/components/ble/common/ble_conn_params.o : $(SDK_ROOT)/components/ble/common/ble_conn_params.c 
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler.o : $(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

$(SDK_ROOT)/components/ble/ble_services/ble_nus/ble_nus.o : $(SDK_ROOT)/components/ble/ble_services/ble_nus/ble_nus.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

##########################################################################
# General Build Rules
##########################################################################
%.o : %.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

%.o : %.s
	@echo $(@F)
	@$(TARGET_AS) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_ASFLAGS) -o $@ $<

%.objdump: %.elf
	@echo $(@F)
	@$(TARGET_OBJDUMP) -dSl -Mforce-thumb $< > $@

%.hex: %.elf
	@echo $(@F)
	@$(TARGET_OBJCOPY) $(TARGET_OCFLAGS) -O ihex $< $@

%.bin: %.elf
	@echo $(@F)
	@$(TARGET_OBJCOPY) $(TARGET_OCFLAGS) -O binary $< $@

-include $(DEPENDS)

endif
