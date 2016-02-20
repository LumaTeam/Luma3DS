rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-ld
OC := arm-none-eabi-objcopy
OPENSSL := openssl

PYTHON3 := python
PYTHON_VER_MAJOR := $(word 2, $(subst ., , $(shell python --version 2>&1)))
ifneq ($(PYTHON_VER_MAJOR), 3)
	PYTHON3 := py -3
endif

name := ReiNand

dir_source := source
dir_data := data
dir_build := build
dir_mset := CakeHax
dir_out := out
dir_emu := emunand
dir_reboot := reboot
dir_ninjhax := CakeBrah

ASFLAGS := -mlittle-endian -mcpu=arm946e-s -march=armv5te
CFLAGS := -Wall -Wextra -MMD -MP -marm $(ASFLAGS) -fno-builtin -fshort-wchar -std=c11 -Wno-main -O2
FLAGS := name=$(name).dat dir_out=$(abspath $(dir_out)) ICON=$(abspath icon.png) --no-print-directory

objects_cfw = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
			  $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
			  $(call rwildcard, $(dir_source), *.s *.c)))


.PHONY: all
all: launcher a9lh emunand emunando3ds reboot rebootntr ninjhax

.PHONY: launcher
launcher: $(dir_out)/$(name).dat 

.PHONY: a9lh
a9lh: $(dir_out)/arm9loaderhax.bin

.PHONY: emunand
emunand: $(dir_out)/rei-n3ds/emunand/emunand.bin

.PHONY: emunando3ds
emunando3ds: $(dir_out)/rei-o3ds/emunand/emunand.bin

.PHONY: reboot
reboot: $(dir_out)/rei-o3ds/reboot/reboot1.bin $(dir_out)/rei-o3ds/reboot/reboot2.bin $(dir_out)/rei-o3ds/reboot/reboot190.bin

.PHONY: rebootntr
rebootntr: $(dir_out)/ntr-o3ds/reboot/reboot1.bin $(dir_out)/ntr-o3ds/reboot/reboot2.bin

.PHONY: ninjhax
ninjhax: $(dir_out)/3ds/$(name)

.PHONY: clean
clean:
	@$(MAKE) $(FLAGS) -C $(dir_mset) clean
	@$(MAKE) $(FLAGS) -C $(dir_ninjhax) clean
	rm -rf $(dir_out) $(dir_build)

$(dir_out)/$(name).dat: $(dir_build)/main.bin $(dir_out)/rei-n3ds/ $(dir_out)/rei-o3ds/
	@$(MAKE) $(FLAGS) -C $(dir_mset) launcher
	dd if=$(dir_build)/main.bin of=$@ bs=512 seek=144

$(dir_out)/arm9loaderhax.bin: $(dir_build)/main.bin $(dir_out)/rei-n3ds/ $(dir_out)/rei-o3ds/
	@cp -av $(dir_build)/main.bin $(dir_out)/arm9loaderhax.bin

$(dir_out)/3ds/$(name):
	@mkdir -p "$(dir_out)/3ds/$(name)"
	@$(MAKE) $(FLAGS) -C $(dir_ninjhax)
	@mv $(dir_out)/$(name).3dsx $@
	@mv $(dir_out)/$(name).smdh $@

$(dir_out)/rei-n3ds/:
	@mkdir -p "$(dir_out)/rei-n3ds"

$(dir_out)/rei-o3ds/:
	@mkdir -p "$(dir_out)/rei-o3ds"
    
$(dir_out)/rei-n3ds/emunand/emunand.bin: $(dir_emu)/emuCode.s
	@armips $<
	@mkdir -p "$(dir_out)/rei-n3ds/emunand"
	@cp -av emunand.bin $(dir_out)/rei-n3ds/emunand

$(dir_out)/rei-o3ds/emunand/emunand.bin: emunand.bin
	@mkdir -p "$(dir_out)/rei-o3ds/emunand"
	@mv emunand.bin $(dir_out)/rei-o3ds/emunand

$(dir_out)/rei-o3ds/reboot/reboot1.bin: $(dir_reboot)/rebootCode.s
	@armips $<
	@mkdir -p "$(dir_out)/rei-o3ds/reboot"
	@mv reboot1.bin $(dir_out)/rei-o3ds/reboot

$(dir_out)/rei-o3ds/reboot/reboot2.bin: reboot2.bin
	@cp -av reboot2.bin $(dir_out)/rei-o3ds/reboot

$(dir_out)/rei-o3ds/reboot/reboot190.bin: $(dir_reboot)/rebootCode90.s
	@armips $<
	@mv reboot1.bin $(dir_out)/rei-o3ds/reboot/reboot190.bin

$(dir_out)/ntr-o3ds/reboot/reboot1.bin: $(dir_reboot)/rebootCodeNtr.s
	@armips $<
	@mkdir -p "$(dir_out)/ntr-o3ds/reboot"
	@mv reboot1.bin $(dir_out)/ntr-o3ds/reboot

$(dir_out)/ntr-o3ds/reboot/reboot2.bin: reboot2.bin
	@mv reboot2.bin $(dir_out)/ntr-o3ds/reboot

$(dir_build)/main.bin: $(dir_build)/main.elf
	$(OC) -S -O binary $< $@

$(dir_build)/main.elf: $(objects_cfw)
	# FatFs requires libgcc for __aeabi_uidiv
	$(CC) -nostartfiles $(LDFLAGS) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<

$(dir_build)/fatfs/%.o: $(dir_source)/fatfs/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) -mthumb -mthumb-interwork -Wno-unused-function $(OUTPUT_OPTION) $<

$(dir_build)/fatfs/%.o: $(dir_source)/fatfs/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) -mthumb -mthumb-interwork $(OUTPUT_OPTION) $<
include $(call rwildcard, $(dir_build), *.d)
