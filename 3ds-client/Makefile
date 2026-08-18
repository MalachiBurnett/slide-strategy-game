#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# Project settings
#---------------------------------------------------------------------------------
TARGET      := slide-3ds
BUILD       := build
SOURCES     := source
DATA        := data
INCLUDES    := include
GRAPHICS    :=
GFXBUILD    := $(BUILD)

APP_TITLE       := Slide
APP_DESCRIPTION := Slide strategy game for 3DS
APP_AUTHOR      := Wiizard Software

APP_TITLEID      := 0x67A4B69C0F0623F3
APP_PRODUCT_CODE := CTR-N-SLIDE

# CIA files
CIA         := $(TARGET).cia
BANNER_PNG  := $(CURDIR)/banner.png
BANNER_WAV  := $(CURDIR)/banner.wav
BANNER_BNR  := $(CURDIR)/banner.bnr
ICON_ICN    := $(CURDIR)/$(TARGET).icn
RSF         := $(CURDIR)/cia.rsf

#---------------------------------------------------------------------------------
# Code generation options
#---------------------------------------------------------------------------------
ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS  := -g -Wall -O2 -mword-relocations \
           -ffunction-sections \
           $(ARCH)

CFLAGS  += $(INCLUDE) -D__3DS__

CXXFLAGS := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++14

ASFLAGS := -g $(ARCH)
LDFLAGS  = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

# curl pulls in mbedtls + zlib; wslay has no extra deps
LIBS := -lcurl \
        -lwslay \
        -lmbedtls -lmbedx509 -lmbedcrypto \
        -lz \
        -lctru -lm

#---------------------------------------------------------------------------------
# Library directories
#---------------------------------------------------------------------------------
LIBDIRS := $(CTRULIB) $(DEVKITPRO)/portlibs/3ds

#---------------------------------------------------------------------------------
# Tool paths
#---------------------------------------------------------------------------------
BANNERTOOL := bannertool
MAKEROM     := makerom

#---------------------------------------------------------------------------------
# No need to edit past this point
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT  := $(CURDIR)/$(TARGET)
export TOPDIR  := $(CURDIR)

export VPATH   := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                  $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR := $(CURDIR)/$(BUILD)

CFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES   := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
    export LD := $(CC)
else
    export LD := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES))
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS := $(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
    icons := $(wildcard *.png)
    ifneq (,$(findstring $(TARGET).png,$(icons)))
        export APP_ICON := $(TOPDIR)/$(TARGET).png
    else ifneq (,$(findstring icon.png,$(icons)))
        export APP_ICON := $(TOPDIR)/icon.png
    endif
else
    export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
    export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

.PHONY: all cia clean

#---------------------------------------------------------------------------------
# Normal 3DSX build
#---------------------------------------------------------------------------------

all: $(BUILD) $(DEPSDIR)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#---------------------------------------------------------------------------------
# CIA build
#---------------------------------------------------------------------------------

cia: all $(CIA)
	@echo "Built $(CIA)"

$(CIA): $(OUTPUT).elf $(OUTPUT).smdh $(BANNER_BNR) $(RSF)
	@echo "Creating $(CIA)..."
	@$(MAKEROM) -f cia \
		-o $@ \
		-rsf $(RSF) \
		-target t \
		-desc app:4 \
		-exefslogo \
		-elf $(OUTPUT).elf \
		-icon $(OUTPUT).smdh \
		-banner $(BANNER_BNR)

#---------------------------------------------------------------------------------
# Banner
#---------------------------------------------------------------------------------

$(BANNER_BNR): $(BANNER_PNG) $(BANNER_WAV)
	@echo "Creating $(notdir $@)..."
	@$(BANNERTOOL) makebanner \
		-i "$<" \
		-a "$(BANNER_WAV)" \
		-o "$@"

#---------------------------------------------------------------------------------

$(BUILD):
	@mkdir -p $@

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
# Clean
#---------------------------------------------------------------------------------

clean:
	@echo clean ...
	@rm -fr $(BUILD) \
		$(TARGET).3dsx \
		$(TARGET).smdh \
		$(TARGET).elf \
		$(TARGET).cia \
		$(TARGET).icn \
		$(BANNER_BNR)

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

$(OUTPUT).3dsx : $(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf : $(OFILES)

#---------------------------------------------------------------------------------
%.bin.o %_bin.h : %.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------