NH_QT_MAJOR ?= 5
NICKELHOOK ?= NickelHook/
include $(patsubst %/,%,$(NICKELHOOK))/NickelHook.mk

override PKGCONF  += Qt$(NH_QT_MAJOR)Widgets
override LIBRARY  := src/libnm.so
override SOURCES  += src/action.c src/action_c.c src/action_cc.cc src/config.c src/generator.c src/generator_c.c src/kfmon.c src/nickelmenu.cc src/util.c
override CFLAGS   += -Wall -Wextra -Werror -fvisibility=hidden
override CXXFLAGS += -Wall -Wextra -Werror -Wno-missing-field-initializers -isystemlib -fvisibility=hidden -fvisibility-inlines-hidden
override CPPFLAGS += -DNM_QT_MAJOR=$(NH_QT_MAJOR)
ifeq ($(NH_QT_MAJOR),6)
override SOURCES += src/install.c $(patsubst %/,%,$(NICKELHOOK))/resources.c
override CFLAGS += -Wno-error=unused-result
else
override KOBOROOT += res/doc:$(NM_CONFIG_DIR)/doc
endif

override SKIPCONFIGURE += strip
strip:
	$(STRIP) --strip-unneeded src/libnm.so
.PHONY: strip

ifeq ($(NM_UNINSTALL_CONFIGDIR),1)
override CPPFLAGS += -DNM_UNINSTALL_CONFIGDIR
endif

ifeq ($(NM_CONFIG_DIR),)
override NM_CONFIG_DIR := /mnt/onboard/.adds/nm
endif

ifneq ($(NM_CONFIG_DIR),/mnt/onboard/.adds/nm)
$(info -- Warning: NM_CONFIG_DIR is set to a non-default value; this will cause issues with other mods using it!)
endif

override CPPFLAGS += -DNM_CONFIG_DIR='"$(NM_CONFIG_DIR)"' -DNM_CONFIG_DIR_DISP='"$(patsubst /mnt/onboard/%,KOBOeReader/%,$(NM_CONFIG_DIR))"'

include $(patsubst %/,%,$(NICKELHOOK))/NickelHook.mk

override GENERATED += test/qt_compat test/qt_compat.d
test/qt_compat: test/qt_compat.cc src/nickelmenu.cc $(filter-out src/nickelmenu.o src/install.o $(NICKELHOOK)nh.o $(NICKELHOOK)resources.o,$(OBJECTS_C) $(OBJECTS_CXX))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fPIC $< $(filter %.o,$^) $(LDFLAGS) -o $@

ifeq ($(NH_QT_MAJOR),6)
override GENERATED += src/embedded_resources.h test/compat test/compat.d
test/compat: test/compat.cc src/compat.h src/compat_data.h src/native.h src/cloud.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fPIC $< $(LDFLAGS) -o $@
src/install.o: src/embedded_resources.h
src/embedded_resources.h: res/doc $(patsubst %/,%,$(NICKELHOOK))/embed_resources.py
	python3 $(patsubst %/,%,$(NICKELHOOK))/embed_resources.py $@ res/doc
endif
