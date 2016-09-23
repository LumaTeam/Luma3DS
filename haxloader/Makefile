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

dir_source := source
dir_cakebrah := CakeBrah
dir_cakehax := CakeHax
dir_cakesrop := CakesROP
dir_build := build
dir_out := ../out

ASFLAGS := -mcpu=arm946e-s
CFLAGS := -Wall -Wextra -MMD -MP -marm $(ASFLAGS) -fno-builtin -fshort-wchar -std=c11 -Wno-main -O2 -flto -ffast-math
LDFLAGS := -nostartfiles
FLAGS := name=$(name).dat dir_out=$(abspath $(dir_out)) ICON=$(abspath icon.png) APP_DESCRIPTION="Noob-friendly 3DS CFW." APP_AUTHOR="Aurora Wright/TuxSH" --no-print-directory
ROP_FLAGS := DATNAME=$(name).dat DISPNAME=$(name).dat GRAPHICS=../data/

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
          $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c)))

.PHONY: all
all: $(dir_out)/$(name).dat $(dir_out)/3ds/$(name) $(dir_out)/menuhax/boot.3dsx $(dir_out)/$(name).nds

.PHONY: clean
clean:
	@$(MAKE) $(FLAGS) -C $(dir_cakebrah) clean
	@$(MAKE) $(FLAGS) -C $(dir_cakehax) clean
	@$(MAKE) $(ROP_FLAGS) -C $(dir_cakesrop) clean
	@rm -rf $(dir_build)

$(dir_out)/$(name).dat: $(dir_build)/main.bin
	@$(MAKE) $(FLAGS) -C $(dir_cakehax) launcher
	@dd if=$(dir_build)/main.bin of=$@ bs=512 seek=144

$(dir_out)/3ds/$(name):
	@mkdir -p "$@"
	@$(MAKE) $(FLAGS) -C $(dir_cakebrah)
	@mv $(dir_out)/$(name).3dsx $(dir_out)/$(name).smdh $@

$(dir_out)/menuhax/boot.3dsx: menuhax.diff
	@mkdir -p "$(@D)"
	@cd $(dir_cakebrah); patch -p1 < ../menuhax.diff; $(MAKE) $(FLAGS); git reset --hard
	@mv $(dir_out)/$(name).3dsx $@
	@rm $(dir_out)/$(name).smdh

$(dir_out)/$(name).nds:
	@$(MAKE) $(ROP_FLAGS) -C $(dir_cakesrop)
	@mv $(dir_cakesrop)/CakesROP.nds $@

$(dir_build)/main.bin: $(dir_build)/main.elf
	$(OC) -S -O binary $< $@

$(dir_build)/main.elf: $(objects) ../$(dir_build)/loader.bin.o
	$(LINK.o) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/memory.o: CFLAGS += -O3

$(dir_build)/%.o: $(dir_source)/%.c $(bundled)
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
include $(call rwildcard, $(dir_build), *.d)
