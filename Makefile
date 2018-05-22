rwildcard = $(foreach d, $(wildcard $1*), $(filter $(subst *, %, $2), $d) $(call rwildcard, $d/, $2))

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

ifneq ($(strip $(shell firmtool -v 2>&1 | grep usage)),)
$(error "Please install firmtool v1.1 or greater")
endif

include $(DEVKITARM)/base_tools

name := Luma3DS
revision := $(shell git describe --tags --match v[0-9]* --abbrev=8 | sed 's/-[0-9]*-g/-/')
version_major := $(shell git describe --tags --match v[0-9]* | cut -c2- | cut -f1 -d- | cut -f1 -d.)
version_minor := $(shell git describe --tags --match v[0-9]* | cut -c2- | cut -f1 -d- | cut -f2 -d.)
version_build := $(shell git describe --tags --match v[0-9]* | cut -c2- | cut -f1 -d- | cut -f3 -d.)
commit := $(shell git rev-parse --short=8 HEAD)
is_release := 0

ifeq ($(strip $(revision)),)
	revision := v0.0.0-0
	version_major := 0
	version_minor := 0
	version_build := 0
endif

ifeq ($(strip $(commit)),)
	commit := 0
endif

ifeq ($(strip $(version_build)),)
	version_build := 0
endif

ifeq ($(strip $(shell git describe --tags --match v[0-9]* | grep -)),)
	is_release := 1
endif

dir_source := source
dir_patches := patches
dir_arm11 := arm11
dir_k11_extension := k11_extension
dir_sysmodules := sysmodules
dir_loader := $(dir_sysmodules)/loader
dir_rosalina := $(dir_sysmodules)/rosalina
dir_sm := $(dir_sysmodules)/sm
dir_pxi := $(dir_sysmodules)/pxi
dir_build := build
dir_out := out

ASFLAGS := -mcpu=arm946e-s
CFLAGS := -Wall -Wextra $(ASFLAGS) -fno-builtin -std=c11 -Wno-main -O2 -ffast-math
LDFLAGS := -nostartfiles -Wl,--nmagic

objects = $(patsubst $(dir_source)/%.s, $(dir_build)/%.o, \
          $(patsubst $(dir_source)/%.c, $(dir_build)/%.o, \
          $(call rwildcard, $(dir_source), *.s *.c)))

bundled = $(dir_build)/reboot.bin.o $(dir_build)/emunand.bin.o

modules = $(dir_build)/loader.cxi $(dir_build)/rosalina.cxi $(dir_build)/sm.cxi $(dir_build)/pxi.cxi

define bin2o
	bin2s $< | $(AS) -o $(@)
endef

.PHONY: all
all: firm

.PHONY: release
release: $(dir_out)/$(name)$(revision).7z

.PHONY: firm
firm: $(dir_out)/boot.firm

.PHONY: clean
clean:
	@$(MAKE) -C $(dir_arm11) clean
	@$(MAKE) -C $(dir_k11_extension) clean
	@$(MAKE) -C $(dir_loader) clean
	@$(MAKE) -C $(dir_rosalina) clean
	@$(MAKE) -C $(dir_sm) clean
	@$(MAKE) -C $(dir_pxi) clean
	@rm -rf $(dir_out) $(dir_build)

.PRECIOUS: $(dir_build)/%.bin

.PHONY: $(dir_arm11)
.PHONY: $(dir_k11_extension)
.PHONY: $(dir_loader)
.PHONY: $(dir_rosalina)
.PHONY: $(dir_sm)
.PHONY: $(dir_pxi)


$(dir_out)/$(name)$(revision).7z: all
	@mkdir -p "$(@D)"
	@[ -f "$@" ] || 7z a -mx $@ ./$(@D)/* ./exception_dump_parser -xr!.DS_Store

$(dir_out)/boot.firm: $(dir_build)/modules.bin $(dir_build)/arm11.elf $(dir_build)/main.elf $(dir_build)/k11_extension.bin
	@mkdir -p "$(@D)"
	@firmtool build $@ -D $^ -A 0x18180000 0x18000000 -C XDMA XDMA NDMA XDMA

$(dir_build)/modules.bin: $(modules)
	@mkdir -p "$(@D)"
	cat $^ > $@

$(dir_build)/arm11.elf: $(dir_arm11)
	@mkdir -p "$(@D)"
	@$(MAKE) -C $<

$(dir_build)/main.elf: $(bundled) $(objects)
	$(LINK.o) -T linker.ld $(OUTPUT_OPTION) $^

$(dir_build)/k11_extension.bin: $(dir_k11_extension)
	@mkdir -p "$(@D)"
	@$(MAKE) -C $<

$(dir_build)/loader.cxi: $(dir_loader)
	@mkdir -p "$(@D)"
	@$(MAKE) -C $<

$(dir_build)/rosalina.cxi: $(dir_rosalina)
	@mkdir -p "$(@D)"
	@$(MAKE) -C $<

$(dir_build)/sm.cxi: $(dir_sm)
	@mkdir -p "$(@D)"
	@$(MAKE) -C $<

$(dir_build)/pxi.cxi: $(dir_pxi)
	@mkdir -p "$(@D)"
	@$(MAKE) -C $<

$(dir_build)/%.bin.o: $(dir_build)/%.bin
	@$(bin2o)

$(dir_build)/%.bin: $(dir_patches)/%.s
	@mkdir -p "$(@D)"
	@armips $<

$(dir_build)/memory.o $(dir_build)/strings.o: CFLAGS += -O3
$(dir_build)/config.o: CFLAGS += -DCONFIG_TITLE="\"$(name) $(revision) configuration\""
$(dir_build)/patches.o: CFLAGS += -DVERSION_MAJOR="$(version_major)" -DVERSION_MINOR="$(version_minor)"\
						-DVERSION_BUILD="$(version_build)" -DISRELEASE="$(is_release)" -DCOMMIT_HASH="0x$(commit)"
$(dir_build)/firm.o: $(dir_build)/modules.bin
$(dir_build)/firm.o: CFLAGS += -DLUMA_SECTION0_SIZE="$(shell wc -c $(dir_build)/modules.bin | tr -d [:space:][:alpha:][:punct:])"

$(dir_build)/bundled.h: $(bundled)
	@$(foreach f, $(bundled),\
	echo "extern const u8" `(echo $(basename $(notdir $(f))) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> $@;\
	echo "extern const u32" `(echo $(basename $(notdir $(f)))| sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> $@;\
	)

$(dir_build)/%.o: $(dir_source)/%.c $(dir_build)/bundled.h
	@mkdir -p "$(@D)"
	$(COMPILE.c) $(OUTPUT_OPTION) $<

$(dir_build)/%.o: $(dir_source)/%.s
	@mkdir -p "$(@D)"
	$(COMPILE.s) $(OUTPUT_OPTION) $<
