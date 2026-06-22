# TARGET: native (default) | moddwarf-new | rpi5
TARGET ?= native

# ── Per-target defaults ────────────────────────────────────────────────────
# override is needed so cross-compilation targets win over the environment CXX.
ifeq ($(TARGET),rpi5)
    override CXX      := aarch64-linux-gnu-g++
    # -fno-tree-vectorize avoids emitting calls to libmvec (vectorized math),
    # which isn't packaged on the RPi5 mod/pistomp Buildroot system.
    override CXXFLAGS := -std=c++17 -O3 -ffast-math -funroll-loops \
                         -fno-tree-vectorize \
                         -mcpu=cortex-a76 -march=armv8.2-a \
                         -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter
    # Static libstdc++/libgcc: embed the C++ runtime so the .so doesn't depend
    # on the target system's libstdc++ version.
    override LDFLAGS  := -static-libstdc++ -static-libgcc
    EXTRA_DEFS = -DDEEPBLUE_DISP_STAGES=16 -DDEEPBLUE_BUBBLE_VOICES=16 \
                 -DDEEPBLUE_REVERB_LINES=8

else ifeq ($(TARGET),moddwarf-new)
    # mod-plugin-builder injects CXX and CXXFLAGS via command-line args.
    # Use ?= so those take precedence; fallbacks serve only for manual builds.
    CXX      ?= aarch64-modaudio-linux-gnu-g++
    CXXFLAGS ?= -std=c++17 -O3 -ffast-math \
                -mcpu=cortex-a35 \
                -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter
    # Short allpass chain, small bubble pool and a 4-line reverb to fit the
    # Cortex-A35 CPU budget.
    EXTRA_DEFS = -DDEEPBLUE_DISP_STAGES=6 -DDEEPBLUE_BUBBLE_VOICES=6 \
                 -DDEEPBLUE_REVERB_LINES=4

else  # native
    CXX      ?= g++
    CXXFLAGS ?= -std=c++17 -O3 -ffast-math \
                -fvisibility=hidden -Wall -Wextra -Wno-unused-parameter
    EXTRA_DEFS =
endif

# In cross-compilation CXXFLAGS already contains -I$(STAGING_DIR)/usr/include
LV2FLAGS ?= $(shell pkg-config --cflags lv2 2>/dev/null)

BUNDLE  = deepblue.lv2
BINARY  = $(BUNDLE)/deepblue.so
SOURCES = src/plugin.cpp src/deepblue_dsp.cpp src/glibc_compat.cpp
HEADERS = src/deepblue_dsp.h src/biquad.hpp src/delayline.hpp src/lfo.hpp src/dispersion.hpp src/bubbles.hpp src/stereofield.hpp src/reverb.hpp

all: $(BINARY)

$(BINARY): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(EXTRA_DEFS) $(LV2FLAGS) -fPIC -shared -o $@ $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(BINARY)

# Recursive copy installs everything inside the bundle: the .so binary and the
# manifest + plugin TTLs (plus presets / modgui once they exist).
install: $(BINARY)
	install -d $(DESTDIR)/usr/lib/lv2
	cp -r $(BUNDLE) $(DESTDIR)/usr/lib/lv2/

.PHONY: all clean install
