rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-ld
OC := arm-none-eabi-objcopy

name := Luma3DS
revision := $(shell git describe --tags --match v[0-9]* --abbrev=8 | sed 's/-[0-9]*-g/-/i')

dir_source := source
dir_patches := patches
dir_loader := loader
dir_screeninit := screeninit
dir_injector := injector
dir_mset := CakeHax
dir_ninjhax := CakeBrah
dir_build := build
dir_out := out

ASFLAGS := -mcpu=arm946e-s
CFLAGS := -Wall -Wextra -MMD -MP -marm $(ASFLAGS) -fno-builtin -fshort-wchar -std=c11 -Wno-main -O2 -flto -ffast-math
LDFLAGS := -nostartfiles
FLAGS := name=$(name).dat dir_out=$(abspath $(dir_out)) ICON=$(abspath icon.png) APP_DESCRIPTION="Noob-friendly 3DS CFW." APP_AUTHOR="Aurora Wright/TuxSH" --no-print-directory

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
          $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c)))

bundled = $(dir_build)/rebootpatch.h $(dir_build)/emunandpatch.h $(dir_build)/injector.h $(dir_build)/loader.h $(dir_build)/screeninit.h

.PHONY: all
all: launcher a9lh ninjhax

.PHONY: launcher
launcher: $(dir_out)/$(name).dat

.PHONY: a9lh
a9lh: $(dir_out)/arm9loaderhax.bin

.PHONY: ninjhax
ninjhax: $(dir_out)/3ds/$(name)

.PHONY: release
release: $(dir_out)/$(name)$(version).7z

.PHONY: clean
clean:
	@$(MAKE) $(FLAGS) -C $(dir_mset) clean
	@$(MAKE) $(FLAGS) -C $(dir_ninjhax) clean
	@$(MAKE) -C $(dir_loader) clean
	@$(MAKE) -C $(dir_screeninit) clean
	@$(MAKE) -C $(dir_injector) clean
	@rm -rf $(dir_out) $(dir_build)

$(dir_out):
	@mkdir -p "$(dir_out)/luma/payloads"

$(dir_out)/$(name).dat: $(dir_build)/main.bin $(dir_out)
	@$(MAKE) $(FLAGS) -C $(dir_mset) launcher
	@dd if=$(dir_build)/main.bin of=$@ bs=512 seek=144

$(dir_out)/arm9loaderhax.bin: $(dir_build)/main.bin $(dir_out)
	@cp -a $(dir_build)/main.bin $@

$(dir_out)/3ds/$(name): $(dir_out)
	@mkdir -p "$@"
	@$(MAKE) $(FLAGS) -C $(dir_ninjhax)
	@mv $(dir_out)/$(name).3dsx $(dir_out)/$(name).smdh $@

$(dir_out)/$(name)$(version).7z: launcher a9lh ninjhax
	@7z a -mx $@ ./$(@D)/*

$(dir_build)/main.bin: $(dir_build)/main.elf
	$(OC) -S -O binary $< $@

$(dir_build)/main.elf: $(objects)
	$(LINK.o) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/emunandpatch.h: $(dir_patches)/emunand.s $(dir_injector)/Makefile
	@mkdir -p "$(@D)"
	@armips $<
	@bin2c -o $@ -n emunand $(@D)/emunand.bin

$(dir_build)/rebootpatch.h: $(dir_patches)/reboot.s
	@mkdir -p "$(@D)"
	@armips $<
	@bin2c -o $@ -n reboot $(@D)/reboot.bin

$(dir_build)/injector.h: $(dir_injector)/Makefile
	@mkdir -p "$(@D)"
	@$(MAKE) -C $(dir_injector)
	@bin2c -o $@ -n injector $(@D)/injector.cxi

$(dir_build)/loader.h: $(dir_loader)/Makefile
	@$(MAKE) -C $(dir_loader)
	@bin2c -o $@ -n loader $(@D)/loader.bin

$(dir_build)/screeninit.h: $(dir_screeninit)/Makefile
	@$(MAKE) -C $(dir_screeninit)
	@bin2c -o $@ -n screeninit $(@D)/screeninit.bin

$(dir_build)/memory.o: CFLAGS += -O3
$(dir_build)/config.o: CFLAGS += -DCONFIG_TITLE="\"$(name) $(revision) configuration\""

$(dir_build)/%.o: $(dir_source)/%.c $(bundled)
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
include $(call rwildcard, $(dir_build), *.d)
