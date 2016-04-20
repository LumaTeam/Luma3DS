rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-ld
OC := arm-none-eabi-objcopy

name := AuReiNand
version := $(shell git describe --abbrev=0 --tags)

dir_source := source
dir_patches := patches
dir_screeninit := screeninit
dir_injector := injector
dir_mset := CakeHax
dir_ninjhax := CakeBrah
dir_build := build
dir_out := out

ASFLAGS := -mcpu=arm946e-s
CFLAGS := -Wall -Wextra -MMD -MP -marm $(ASFLAGS) -fno-builtin -fshort-wchar -std=c11 -Wno-main -O2 -flto -ffast-math
LDFLAGS := -nostartfiles
FLAGS := name=$(name).dat dir_out=$(abspath $(dir_out)) ICON=$(abspath icon.png) APP_DESCRIPTION="Noob-friendly 3DS CFW." APP_AUTHOR="Reisyukaku/Aurora Wright" --no-print-directory

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
          $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c)))

bundled = $(dir_build)/patches.h $(dir_build)/screeninit.h

.PHONY: all
all: a9lh pathchanger

.PHONY: a9lh
a9lh: $(dir_out)/ShadowFW.bin

.PHONY: release
release: $(dir_out)/$(name).zip

.PHONY: pathchanger
pathchanger: $(dir_out)/pathchanger

.PHONY: clean
clean:
	@$(MAKE) -C $(dir_screeninit) clean
	@$(MAKE) -C $(dir_injector) clean
	@rm -rf $(dir_out) $(dir_build)

$(dir_out):
	@mkdir -p "$(dir_out)"

$(dir_out)/pathchanger: $(dir_out)
	@cc pathchanger/pathchanger.c -o out/pathchanger

$(dir_out)/ShadowFW.bin: $(dir_build)/main.bin $(dir_out)
	@cp -a $(dir_build)/main.bin $@

$(dir_out)/$(name).zip: launcher a9lh
	@cd "$(@D)" && zip -9 -r $(name) *

$(dir_build)/main.bin: $(dir_build)/main.elf
	$(OC) -S -O binary $< $@

$(dir_build)/main.elf: $(objects)
	$(LINK.o) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/patches.h: $(dir_patches)/emunand.s $(dir_patches)/reboot.s $(dir_injector)/Makefile
	@mkdir -p "$(@D)"
	@armips $<
	@armips $(word 2,$^)
	@$(MAKE) -C $(dir_injector)
	@mv emunand.bin reboot.bin $(dir_injector)/injector.cxi $(@D)
	@bin2c -o $@ -n emunand $(@D)/emunand.bin -n reboot $(@D)/reboot.bin -n injector $(@D)/injector.cxi

# $(dir_build)/loader.h: $(dir_loader)/Makefile
# 	@$(MAKE) -C $(dir_loader)
# 	@mv $(dir_loader)/loader.bin $(@D)
# 	@bin2c -o $@ -n loader $(@D)/loader.bin

$(dir_build)/screeninit.h: $(dir_screeninit)/Makefile
	@$(MAKE) -C $(dir_screeninit)
	@mv $(dir_screeninit)/screeninit.bin $(@D)
	@bin2c -o $@ -n screeninit $(@D)/screeninit.bin

$(dir_build)/memory.o : CFLAGS += -O3
$(dir_build)/config.o : CFLAGS += -DCONFIG_TITLE="\"$(name) $(version) configuration\""

$(dir_build)/%.o: $(dir_source)/%.c $(bundled)
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<

$(dir_build)/fatfs/%.o: $(dir_source)/fatfs/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/fatfs/%.o: $(dir_source)/fatfs/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
include $(call rwildcard, $(dir_build), *.d)
