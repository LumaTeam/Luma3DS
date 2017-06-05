rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_tools

name := kernel_extension

dir_source := source
dir_include := include
dir_build := build

ARCH    := -mcpu=mpcore -mfpu=vfp
ASFLAGS := $(ARCH)
CFLAGS  := -Wall -Wextra -MMD -MP -marm $(ASFLAGS) -I$(dir_include) -fno-builtin -std=c11 -Wno-main -O2 -flto -ffast-math \
-mword-relocations -ffunction-sections -fdata-sections
LDFLAGS := -nostdlib -Wl,--gc-sections $(ARCH)

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
		$(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
		$(call rwildcard, $(dir_source), *.s *.c)))

.PHONY: all
all: ../$(dir_build)/$(name).bin

.PHONY: clean
clean:
	@rm -rf $(dir_build)

../$(dir_build)/$(name).bin: $(dir_build)/$(name).elf
	$(OBJCOPY) -S -O binary $< $@

$(dir_build)/$(name).elf: $(objects)
	$(CC) $(LDFLAGS) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/memory.o : CFLAGS += -O3 -marm

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
include $(call rwildcard, $(dir_build), *.d)
