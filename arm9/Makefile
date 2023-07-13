#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/base_rules

export REVISION			:=	$(shell git describe --tags --match v[0-9]* --abbrev=8 | sed 's/-[0-9]*-g/-/')
export VERSION_MAJOR	:=	$(shell git describe --tags --match v[0-9]* | cut -c2- | cut -f1 -d- | cut -f1 -d.)
export VERSION_MINOR	:=	$(shell git describe --tags --match v[0-9]* | cut -c2- | cut -f1 -d- | cut -f2 -d.)
export VERSION_BUILD	:=	$(shell git describe --tags --match v[0-9]* | cut -c2- | cut -f1 -d- | cut -f3 -d.)
export COMMIT			:=	$(shell git rev-parse --short=8 HEAD)
export IS_RELEASE		:=	0

ifeq ($(strip $(REVISION)),)
	export REVISION			:=	v0.0.0-0
	export VERSION_MAJOR	:=	0
	export VERSION_MINOR	:=	0
	export VERSION_BUILD	:=	0
endif

ifeq ($(strip $(COMMIT)),)
	export COMMIT			:=	0
endif

ifeq ($(strip $(VERSION_BUILD)),)
	export VERSION_BUILD	:=	0
endif

ifeq ($(strip $(shell git describe --tags --match v[0-9]* | grep -)),)
	export IS_RELEASE		:=	1
endif

# Default 3DSX TitleID for hb:ldr (note: also defined in top-level Makefile)
export HBLDR_DEFAULT_3DSX_TID ?= 000400000D921E00

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#---------------------------------------------------------------------------------
APP_TITLE	:=	Luma3DS
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	source source/fatfs source/fatfs/sdmmc
DATA		:=	data
INCLUDES	:=	include

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-marm -march=armv5te -mtune=arm946e-s

ifeq ($(BUILD_FOR_EXPLOIT_DEV),1)
	DEFINES :=	-DARM9 -D__3DS__ -DHBLDR_DEFAULT_3DSX_TID="0x$(HBLDR_DEFAULT_3DSX_TID)ULL" -DBUILD_FOR_EXPLOIT_DEV=1
else
	DEFINES :=	-DARM9 -D__3DS__ -DHBLDR_DEFAULT_3DSX_TID="0x$(HBLDR_DEFAULT_3DSX_TID)ULL"
endif

FALSEPOSITIVES := -Wno-array-bounds -Wno-stringop-overflow -Wno-stringop-overread
CFLAGS	:=	-g -std=gnu11 -Wall -Wextra -Werror -O2 -mword-relocations \
			-fomit-frame-pointer -ffunction-sections -fdata-sections \
			-Wno-main $(FALSEPOSITIVES) $(ARCH) $(DEFINES)

CFLAGS	+=	$(INCLUDE)

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=$(TOPDIR)/linker.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:=

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:=


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

export APP_TITLE	:=	$(notdir $(TOPDIR)/..)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 		:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#---------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).bin $(TARGET).elf


#---------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
all	:	$(OUTPUT).elf

$(OUTPUT).elf	:	$(OFILES)

%.elf: $(OFILES)
	@echo linking $(notdir $@)
	@$(LD) $(LDFLAGS) $(OFILES) $(LIBPATHS) $(LIBS) -o $@
	@$(NM) -CSn $@ > $(notdir $*.lst)

$(OFILES_SRC)	: $(HFILES_BIN)

memory.o strings.o:	CFLAGS +=	-O3
patches.o config.o:	CFLAGS +=	-DCONFIG_TITLE="\"$(APP_TITLE) $(REVISION) configuration\""\
								-DVERSION_MAJOR="$(VERSION_MAJOR)" -DVERSION_MINOR="$(VERSION_MINOR)"\
								-DVERSION_BUILD="$(VERSION_BUILD)" -DISRELEASE="$(IS_RELEASE)" -DCOMMIT_HASH="0x$(COMMIT)"
config.o ini.o:		CFLAGS +=	-DINI_HANDLER_LINENO=1 -DINI_STOP_ON_FIRST_ERROR=1
#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)
#---------------------------------------------------------------------------------
%.ini.o	%_ini.h:	%.ini
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
