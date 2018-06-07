# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, you can obtain one at http://mozilla.org/MPL/2.0/

##########################################################################
# User configuration and firmware specific object files
##########################################################################
NAME = nrf51_tinydfu
SHELL = /bin/bash

#SDK_ROOT = ../../../../mnt/d/nrfsdk10  # Windows
SDK_ROOT = ../../Downloads/nRF51_SDK_10 # Mac

# Debugging stuff
TERMINAL ?= xterm -e
JLINKEXE := JLinkExe
JLINKGDB := JLinkGDBServer
JLINK_OPTIONS = -device nrf51822 -if swd -speed 1000

# Flash start address
FLASH_START_ADDRESS := 0x0003C000

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

# Softdevice / nRF SDK includes
INCLUDES += \
  $(SDK_ROOT)/components/libraries/experimental_section_vars \
  $(SDK_ROOT)/components \
  $(SDK_ROOT)/components/toolchain \
  $(SDK_ROOT)/components/toolchain/gcc \
  $(SDK_ROOT)/components/toolchain/cmsis/include \
  $(SDK_ROOT)/components/device \
  $(SDK_ROOT)/components/softdevice/s110/headers \
  $(SDK_ROOT)/components/softdevice/s110/headers/nrf51 \
  $(SDK_ROOT)/components/softdevice/common/softdevice_handler \
  $(SDK_ROOT)/components/ble/common \
  $(SDK_ROOT)/components/ble/ble_radio_notification \
  $(SDK_ROOT)/components/ble/nrf_ble_qwr \
  $(SDK_ROOT)/components/ble/ble_racp \
  $(SDK_ROOT)/components/ble/ble_advertising \
  $(SDK_ROOT)/components/ble/ble_dtm \
  $(SDK_ROOT)/components/ble/peer_manager \
  $(SDK_ROOT)/components/ble/ble_services/ble_nus_c \
  $(SDK_ROOT)/components/ble/ble_services/ble_nus \
  $(SDK_ROOT)/components/drivers_nrf/comp \
  $(SDK_ROOT)/components/drivers_nrf/hal \
  $(SDK_ROOT)/components/drivers_nrf/common \
  $(SDK_ROOT)/components/drivers_nrf/delay \
  $(SDK_ROOT)/components/drivers_nrf/timer \
  $(SDK_ROOT)/components/drivers_nrf/power \
  $(SDK_ROOT)/components/drivers_nrf/pstorage \
  $(SDK_ROOT)/components/libraries/fstorage \
  $(SDK_ROOT)/components/libraries/util \
  $(SDK_ROOT)/components/libraries/timer \
  $(SDK_ROOT)/components/libraries/trace \
  $(SDK_ROOT)/components/libraries/scheduler \

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

COMMON_FLAGS := -g -c -Wall -Werror -ffunction-sections -fdata-sections -fno-strict-aliasing \
	-fmessage-length=0 -std=gnu11 \
	-DTARGET=NRF51 -DNRF51 -DS110 -DBLE_STACK_SUPPORT_REQD -DNRF_SD_BLE_API_VERSION=2 -DSOFTDEVICE_PRESENT \
	--short-enums -fno-builtin \
	-DPROGRAM_VERSION=\"$(PROGRAM_VERSION)\" \
	-DNO_VTOR_CONFIG \
	-DDEBUG

#	-DSEMIHOSTED \

COMMON_ASFLAGS := -D__ASSEMBLY__ -x assembler-with-cpp

TARGET_ARCHFLAGS := -march=armv6-m -mthumb -mcpu=cortex-m0 -mfloat-abi=soft 
TARGET_FLAGS := $(COMMON_FLAGS) -Og -g -fmerge-constants $(TARGET_ARCHFLAGS) $(INCLUDE)
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
  ../$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler.c \
  ../$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler_appsh.c \
  ../$(SDK_ROOT)/components/libraries/timer/app_timer.c \
  ../$(SDK_ROOT)/components/libraries/util/app_util_platform.c \
  ../$(SDK_ROOT)/components/libraries/fstorage/fstorage.c \
  ../$(SDK_ROOT)/components/drivers_nrf/pstorage/pstorage.c \
  ../$(SDK_ROOT)/components/ble/common/ble_conn_params.c \
  ../$(SDK_ROOT)/components/ble/common/ble_advdata.c \
  ../$(SDK_ROOT)/components/ble/common/ble_srv_common.c \
  ../$(SDK_ROOT)/components/ble/ble_radio_notification/ble_radio_notification.c \
  ../$(SDK_ROOT)/components/ble/ble_services/ble_nus/ble_nus.c \
  ../$(SDK_ROOT)/components/ble/ble_advertising/ble_advertising.c \
  ../$(SDK_ROOT)/components/libraries/scheduler/app_scheduler.c 


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
	@rm -f ../$(SDK_ROOT)/components/ble/common/ble_conn_params.o
	@rm -f ../$(SDK_ROOT)/components/ble/common/ble_advdata.o
	@rm -f ../$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler.o
	@rm -f ../$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler_appsh.o
	@rm -f ../$(SDK_ROOT)/components/ble/ble_services/ble_nus/ble_nus.o
	@rm -f ../$(SDK_ROOT)/components/ble/ble_advertising/ble_advertising.o
	@rm -f ../$(SDK_ROOT)/components/libraries/timer/app_timer.o
	@rm -f ../$(SDK_ROOT)/components/ble/common/ble_srv_common.o
	@rm -f ../$(SDK_ROOT)/components/ble/ble_radio_notification/ble_radio_notification.o
	@rm -f ../$(SDK_ROOT)/components/libraries/util/app_util_platform.o
	@rm -f ../$(SDK_ROOT)/components/libraries/fstorage/fstorage.o
	@rm -f ../$(SDK_ROOT)/components/drivers_nrf/pstorage/pstorage.o
	@rm -f ../$(SDK_ROOT)/components/libraries/scheduler/app_scheduler.o

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
### I try to only include what's absolutely neccessary. Spaghetti SDK.
###
../$(SDK_ROOT)/components/ble/common/ble_conn_params.o : ../$(SDK_ROOT)/components/ble/common/ble_conn_params.c 
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/ble/common/ble_advdata.o : ../$(SDK_ROOT)/components/ble/common/ble_advdata.c 
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler.o : ../$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler_appsh.o : ../$(SDK_ROOT)/components/softdevice/common/softdevice_handler/softdevice_handler_appsh.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/ble/ble_services/ble_nus/ble_nus.o : ../$(SDK_ROOT)/components/ble/ble_services/ble_nus/ble_nus.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/ble/ble_advertising/ble_advertising.o : ../$(SDK_ROOT)/components/ble/ble_advertising/ble_advertising.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/libraries/timer/app_timer.o : ../$(SDK_ROOT)/components/libraries/timer/app_timer.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/ble/common/ble_srv_common.o : ../$(SDK_ROOT)/components/ble/common/ble_srv_common.c 
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/ble/ble_radio_notification/ble_radio_notification.o : ../$(SDK_ROOT)/components/ble/ble_radio_notification/ble_radio_notification.c 
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/libraries/util/app_util_platform.o : ../$(SDK_ROOT)/components/libraries/util/app_util_platform.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/libraries/fstorage/fstorage.o : ../$(SDK_ROOT)/components/libraries/fstorage/fstorage.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/drivers_nrf/pstorage/pstorage.o : ../$(SDK_ROOT)/components/drivers_nrf/pstorage/pstorage.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<

../$(SDK_ROOT)/components/libraries/scheduler/app_scheduler.o : ../$(SDK_ROOT)/components/libraries/scheduler/app_scheduler.c
	@echo $(@F)
	@$(TARGET_CC) -MMD -MP -MF $(DEPSDIR)/$*.d $(TARGET_CFLAGS) -o $@ $<



#########################################################################
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
