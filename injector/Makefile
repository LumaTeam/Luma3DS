rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/3ds_rules

CC := arm-none-eabi-gcc
AS := arm-none-eabi-as
LD := arm-none-eabi-ld
OC := arm-none-eabi-objcopy

name := $(shell basename $(CURDIR))

dir_source := source
dir_build := build

LIBS := -lctru
LIBDIRS	:= $(CTRULIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

INCLUDE	:= $(foreach dir,$(LIBDIRS),-I$(dir)/include)

ARCH := -mcpu=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS := -Wall -Wextra -MMD -MP -marm $(ARCH) -fno-builtin -std=c11 -O2 -flto -ffast-math -mword-relocations \
	  -ffunction-sections -fdata-sections $(INCLUDE) -DARM11 -D_3DS
LDFLAGS := -Xlinker --defsym="__start__=0x14000000" -specs=3dsx.specs $(ARCH)

objects = $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.c))

.PHONY: all
all: $(name).cxi

.PHONY: clean
clean:
	@rm -rf $(dir_build)

$(name).cxi: $(dir_build)/$(name).elf
	@makerom -f ncch -rsf loader.rsf -nocodepadding -o $@ -elf $<

$(dir_build)/$(name).elf: $(objects)
	$(LINK.o) $(OUTPUT_OPTION) $^ $(LIBPATHS) $(LIBS)

$(dir_build)/memory.o : CFLAGS += -O3

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<
include $(call rwildcard, $(dir_build), *.d)
