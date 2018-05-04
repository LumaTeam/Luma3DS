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
dir_include := include
dir_build := build
dir_out := ../../$(dir_build)

LIBS := -lctru
LIBDIRS	:= $(CTRULIB)
LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

INCLUDE	:= $(foreach dir,$(LIBDIRS),-I$(dir)/include)

ARCH := -mcpu=mpcore -mfloat-abi=hard
ASFLAGS := -g $(ARCH)
CFLAGS := -Wall -Wextra -MMD -MP -marm $(ASFLAGS) -mtp=soft -fno-builtin -std=c11 -O2 -flto -ffast-math -mword-relocations \
		-fomit-frame-pointer -ffunction-sections -fdata-sections $(INCLUDE) -I$(dir_include) -DARM11 -D_3DS
LDFLAGS := -specs=3dsx.specs -g $(ARCH) -mtp=soft -Wl,--section-start,.text=0x14000000 -Wl,--gc-sections

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
		$(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
		$(call rwildcard, $(dir_source), *.s *.c)))

xml_files = $(call rwildcard, $(dir_source), *.xml)

.PHONY: all
all: $(dir_out)/$(name).cxi

.PHONY: clean
clean:
	@rm -rf $(dir_build)

$(dir_out)/$(name).cxi: $(dir_build)/$(name).elf
	@makerom -f ncch -rsf rosalina.rsf -nocodepadding -o $@ -elf $<

$(dir_build)/$(name).elf: $(objects)
	$(LINK.o) $(OUTPUT_OPTION) $^ $(LIBPATHS) $(LIBS)

$(dir_build)/xml_data.h: $(xml_files)
	@ echo "" > $(@)
	@$(foreach f, $(xml_files),\
	echo "static const char" `(echo $(notdir $(f)) | sed -e 's/^\([0-9]\)/_\1/g' | tr . _)`"[] = " >> $(@);\
	sed -e 's/\\/\\\\/g;s/"/\\"/g;s/^/"/g;s/[ \t\r\n]*$$/\\n"/g' $(f) >> $(@);\
	echo ';' >> $@;\
	)
$(dir_build)/gdb/xfer.o: $(dir_build)/xml_data.h
$(dir_build)/memory.o : CFLAGS += -O3

$(dir_build)/%.o: $(dir_source)/%.c
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
include $(call rwildcard, $(dir_build), *.d)
