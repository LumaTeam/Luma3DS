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

dir_source := source
dir_data := data
dir_build := build
dir_mset := mset
dir_out := out
dir_emu := emunand
dir_thread := thread
dir_ninjhax := ninjhax

ASFLAGS := -mlittle-endian -mcpu=arm946e-s -march=armv5te
CFLAGS := -MMD -MP -marm $(ASFLAGS) -fno-builtin -fshort-wchar -std=c11 -Wno-main
FLAGS := dir_out=$(abspath $(dir_out)) 

objects_cfw = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
			  $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
			  $(call rwildcard, $(dir_source), *.s *.c)))


.PHONY: all
all: launcher emunand thread ninjhax

.PHONY: launcher
launcher: $(dir_out)/ReiNand.dat 

.PHONY: emunand
emunand: $(dir_out)/rei/emunand/emunand.bin

.PHONY: thread
thread: $(dir_out)/rei/thread/arm9.bin

.PHONY: ninjhax
ninjhax: $(dir_out)/3ds/ReiNand

.PHONY: clean
clean:
	@$(MAKE) $(FLAGS) -C $(dir_mset) clean
	@$(MAKE) $(FLAGS) -C $(dir_ninjhax) clean
	rm -rf $(dir_out) $(dir_build)

.PHONY: $(dir_out)/ReiNand.dat
$(dir_out)/ReiNand.dat: $(dir_build)/main.bin $(dir_out)/rei/
	@$(MAKE) $(FLAGS) -C $(dir_mset) launcher
	dd if=$(dir_build)/main.bin of=$@ bs=512 seek=256
    
$(dir_out)/3ds/ReiNand:
	@mkdir -p "$(dir_out)/3ds/ReiNand"
	@$(MAKE) -C $(dir_ninjhax)
	@cp -av $(dir_ninjhax)/ReiNand.3dsx $@
	@cp -av $(dir_ninjhax)/ReiNand.smdh $@
    
$(dir_out)/rei/: $(dir_data)/firmware.bin $(dir_data)/splash.bin
	@mkdir -p "$(dir_out)/rei"
	@cp -av $(dir_data)/*bin $@
    
$(dir_out)/rei/thread/arm9.bin: $(dir_thread)
	@$(MAKE) $(FLAGS) -C $(dir_thread)
	@mkdir -p "$(dir_out)/rei/thread"
	@mv $(dir_thread)/arm9.bin $(dir_out)/rei/thread
    
$(dir_out)/rei/emunand/emunand.bin: $(dir_emu)/emuCode.s
	@armips $<
	@mkdir -p "$(dir_out)/rei/emunand"
	@mv emunand.bin $(dir_out)/rei/emunand

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
