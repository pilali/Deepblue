################################################################################
# deepblue — LV2 "underwater" guitar effect for MOD Dwarf / Raspberry Pi 5
#
# To update: set DEEPBLUE_VERSION to the desired commit hash, then rebuild.
################################################################################

DEEPBLUE_VERSION = 98239231a335ca64d3a93d8c59380aa2fb3b9113
DEEPBLUE_SITE    = $(call github,pilali,deepblue,$(DEEPBLUE_VERSION))
DEEPBLUE_BUNDLES = deepblue.lv2

# Longer dispersion chain on the RPi5 (Cortex-A76); short chain on the MOD
# Dwarf (Cortex-A35) to stay within its CPU budget.
ifeq ($(BR2_cortex_a76),y)
DEEPBLUE_DISP_DEFS = -DDEEPBLUE_DISP_STAGES=16 -DDEEPBLUE_BUBBLE_VOICES=16
else
DEEPBLUE_DISP_DEFS = -DDEEPBLUE_DISP_STAGES=6 -DDEEPBLUE_BUBBLE_VOICES=6
endif

define DEEPBLUE_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) \
		TARGET=moddwarf-new \
		CXX="$(TARGET_CXX)" \
		STRIP="$(TARGET_STRIP)" \
		CXXFLAGS="$(TARGET_CXXFLAGS) -std=c++17 -O3 -ffast-math -fvisibility=hidden $(DEEPBLUE_DISP_DEFS)"
endef

define DEEPBLUE_INSTALL_TARGET_CMDS
	cp -r $(@D)/deepblue.lv2 $(TARGET_DIR)/usr/lib/lv2/
endef

$(eval $(generic-package))
