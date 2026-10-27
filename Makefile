# Nuvio Wii U - Makefile (devkitPPC + wut)
ifeq ($(strip $(DEVKITPRO)),)
$(error "Defina DEVKITPRO")
endif
include $(DEVKITPRO)/wut/share/wut_rules

# Precisa vir DEPOIS do include (as regras do devkitPro usam TAB)
.RECIPEPREFIX := >

TARGET   := nuvio
SRC      := source
PKGCFG   := $(DEVKITPRO)/portlibs/wiiu/bin/powerpc-eabi-pkg-config

CXXFILES := $(wildcard $(SRC)/*.cpp)
OFILES   := $(CXXFILES:.cpp=.o)

CXXFLAGS := -g -O2 -Wall -std=gnu++17 -ffunction-sections $(MACHDEP) \
            $(shell $(PKGCFG) --cflags sdl2 SDL2_ttf libcurl) \
            -I$(WUT_ROOT)/include -D__WIIU__ -D__WUT__
LDFLAGS  := -g $(ARCH) $(RPXSPECS) -L$(WUT_ROOT)/lib -Wl,-Map,$(TARGET).map
LIBS     := $(shell $(PKGCFG) --libs --static sdl2 SDL2_ttf libcurl) -lwut

all: $(TARGET).rpx

$(SRC)/%.o: $(SRC)/%.cpp
> $(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: $(OFILES)
> $(CXX) $(LDFLAGS) $(OFILES) $(LIBS) -o $@

clean:
> rm -f $(OFILES) $(TARGET).elf $(TARGET).rpx $(TARGET).map

.PHONY: all clean
