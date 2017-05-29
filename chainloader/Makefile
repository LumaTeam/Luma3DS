rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_tools

name := $(shell basename $(CURDIR))

dir_source := source
dir_build := build
dir_out := ../$(dir_build)

ASFLAGS := -mcpu=arm946e-s
CFLAGS  := -Wall -Wextra -marm $(ASFLAGS) -fno-builtin -std=c11 -Wno-main -O2 -flto -ffast-math
LDFLAGS := -nostartfiles -Wl,--nmagic

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
          $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c)))

.PHONY: all
all: $(dir_out)/$(name).bin

.PHONY: clean
clean:
	@rm -rf $(dir_build)

$(dir_out)/$(name).bin: $(dir_build)/$(name).elf
	$(OBJCOPY) -S -O binary $< $@

$(dir_build)/$(name).elf: $(objects)
	$(LINK.o) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/memory.o: CFLAGS += -O3

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
