.DEFAULT_GOAL := all

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is unset. Open the devkitPro MSYS2 shell)
endif
ifeq ($(wildcard $(DEVKITPPC)/wii_rules),)
$(error Wii build rules are missing. Check DEVKITPPC and the devkitPPC installation)
endif

include $(DEVKITPPC)/wii_rules

SOURCE_DIR := $(if $(wildcard source/main.cpp),source,.)
SOURCES := $(wildcard $(SOURCE_DIR)/*.cpp)
HEADERS := $(wildcard $(SOURCE_DIR)/*.h)

.PHONY: all newspaper clean
all: wii-game.dol

newspaper: wii-newspaper.dol

wii-game.dol: wii-game.elf
wii-newspaper.dol: wii-newspaper.elf

wii-newspaper.elf: PREVIEW_FLAG := -DPREVIEW_NEWSPAPER

wii-game.elf wii-newspaper.elf: $(SOURCES) $(HEADERS) $(firstword $(MAKEFILE_LIST))
	$(CXX) $(MACHDEP) $(PREVIEW_FLAG) -O2 -Wall -Wextra -I"$(LIBOGC_INC)" $(SOURCES) \
		-L"$(LIBOGC_LIB)" -lwiiuse -lbte -logc -lm -o $@

clean:
	rm -f wii-game.elf wii-game.dol wii-newspaper.elf wii-newspaper.dol
