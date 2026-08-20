#---------------------------------------------------------------------------------
# lava-dome -- a magnolia game
#---------------------------------------------------------------------------------
.SUFFIXES:
.SECONDARY:
#---------------------------------------------------------------------------------
# The host tests deliberately need no cross-compiler: requiring devkitPPC to run
# them would put them out of reach on the machine where they are most useful.
HOST_TESTS := test-hand-eval test-dome

ifeq ($(filter test $(HOST_TESTS),$(MAKECMDGOALS)),)
ifeq ($(strip $(DEVKITPPC)),)
$(error "Please set DEVKITPPC. export DEVKITPPC=<path to>devkitPPC")
endif

include $(DEVKITPPC)/wii_rules
endif

#---------------------------------------------------------------------------------
TARGET      := lava-dome
BUILD       := build
SOURCES     := source ../magnolia/source ../magnolia/font
DATA        :=
INCLUDES    := ../magnolia ../magnolia/source ../magnolia/font source

CFLAGS      = -g -O2 -Wall $(MACHDEP) $(INCLUDE)
CXXFLAGS    = $(CFLAGS)
LDFLAGS     = -g $(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS        := -lgrrlib -lpngu -lfreetype -lpng -ljpeg -lz -lbrotlidec -lbrotlicommon -lbz2 -lfat -lasnd -lwiiuse -lbte -logc -lm
LIBDIRS     := $(PORTLIBS)

#---------------------------------------------------------------------------------
ifeq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

TOPDIR := $(CURDIR)/..

export OUTPUT   := $(CURDIR)/$(TARGET)
export VPATH    := $(foreach dir,$(SOURCES),$(TOPDIR)/$(dir))
export DEPSDIR  := $(CURDIR)

# Sprites and audio are linked into the binary rather than read from SD. Assets on
# a card can go missing, go stale, or disagree with the code -- all three have
# happened. bin2s emits one assembly blob plus a header of symbols; -a 32 matches
# ASND's DMA alignment so audio plays straight out of the image with no copy.
BIN2S       := $(DEVKITPRO)/tools/bin/bin2s
ASSETFILES  := $(wildcard $(TOPDIR)/sprites/*.png) $(wildcard $(TOPDIR)/audio/*.pcm)

CFILES      := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.c)))
CPPFILES    := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(TOPDIR)/$(dir)/*.cpp)))

ifeq ($(strip $(CPPFILES)),)
    export LD  := $(CC)
else
    export LD  := $(CXX)
endif

export OFILES   := assets.o $(addsuffix .o,$(basename $(CPPFILES) $(CFILES)))
export INCLUDE  := $(foreach dir,$(INCLUDES),-I$(TOPDIR)/$(dir)) \
                   $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                   -I$(CURDIR) -I$(CURDIR)/$(BUILD) -I$(LIBOGC_INC)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) -L$(LIBOGC_LIB)

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

assets.s: $(ASSETFILES)
	@echo "embedding $(words $(ASSETFILES)) assets ..."
	@$(BIN2S) -a 32 -H assets.h $(ASSETFILES) > assets.s
	@echo "" >> assets.s

assets.h: assets.s

assets.o: assets.s
	@$(AS) -o $@ assets.s

# Every translation unit may include assets.h, so it must exist first.
$(filter-out assets.o,$(OFILES)): assets.h

-include $(DEPENDS)

#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------

.PHONY: $(BUILD) clean all deploy dolphin test $(HOST_TESTS)

# Stated explicitly: the host-test rule below would otherwise be the first
# target in the file, and a bare `make` would run the tests instead of
# building the game.
.DEFAULT_GOAL := all

# Host-side tests. Anything in source/ that is free of libogc can be tested on
# the machine you are sitting at, in a second, with no emulator in the loop --
# which is where game rules belong, because a rule that is subtly wrong looks
# exactly like a rule that is right until someone plays far enough to notice.
#
# One binary per test file, each naming what it links, the same shape magnolia
# uses. The list is the record of which modules are host-clean; a wildcard over
# source/ would try to link the rendering and fail with something far less
# informative.
#
# -O2 is not incidental: test_hand_eval enumerates all 2,598,960 five-card
# hands, and the difference between an optimised and unoptimised sweep is the
# difference between a test you run on every build and one you start avoiding.
HOSTCC     ?= cc
HOSTCFLAGS := -Wall -Wextra -O2 -I source -I tests

# Note the plain mkdir rather than an order-only dependency on $(BUILD): here
# $(BUILD) is the target that cross-compiles the game, not merely a directory,
# so depending on it would drag devkitPPC into `make test` and undo the one
# property these tests exist for.
test: test-hand-eval test-dome

test-hand-eval:
	@mkdir -p $(BUILD)
	@$(HOSTCC) $(HOSTCFLAGS) -o $(BUILD)/$@ \
	    tests/test_hand_eval.c source/hand_eval.c source/cards.c
	@$(BUILD)/$@

test-dome:
	@mkdir -p $(BUILD)
	@$(HOSTCC) $(HOSTCFLAGS) -o $(BUILD)/$@ \
	    tests/test_dome.c source/dome.c source/hand_eval.c source/cards.c
	@$(BUILD)/$@

all: $(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).elf $(TARGET).dol sdcard

# Stages a Homebrew Channel app directory. Assets are in the binary, so this is
# boot.dol and meta.xml -- but the directory is cleared rather than merged,
# because a leftover file from an older layout is exactly what makes a card
# ambiguous to debug.
SDROOT := sdcard/apps/$(TARGET)
deploy: $(BUILD)
	@rm -rf $(SDROOT)
	@mkdir -p $(SDROOT)
	@cp $(BUILD)/$(TARGET).dol $(SDROOT)/boot.dol
	@cp meta.xml $(SDROOT)/meta.xml
	@echo "Staged $(SDROOT)/ -- copy sdcard/ to the card root"

# Pushes the staged directory to the folder Dolphin reads as its SD card.
# No-ops harmlessly on a machine without /mnt/c.
DOLPHIN_SD ?= /mnt/c/Dolphin/sdcard
dolphin: deploy
	@if [ -d "$(dir $(DOLPHIN_SD))" ]; then \
	    rm -rf "$(DOLPHIN_SD)/apps/$(TARGET)"; \
	    mkdir -p "$(DOLPHIN_SD)/apps/$(TARGET)"; \
	    cp -r $(SDROOT)/. "$(DOLPHIN_SD)/apps/$(TARGET)/"; \
	    echo "Synced to $(DOLPHIN_SD)/apps/$(TARGET)/"; \
	else \
	    echo "No $(DOLPHIN_SD) here -- skipping"; \
	fi
	@echo ""
	@echo "NOTE: this clears the app directory, which deletes saved scores and"
	@echo "settings with it. Expected on a dev loop; do not mistake it for the"
	@echo "game failing to save."

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
