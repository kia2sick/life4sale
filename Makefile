.DEFAULT_GOAL := all

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is unset. Open the devkitPro MSYS2 shell)
endif
ifeq ($(wildcard $(DEVKITPPC)/wii_rules),)
$(error Wii build rules are missing. Check DEVKITPPC and the devkitPPC installation)
endif

include $(DEVKITPPC)/wii_rules

MAIN := $(firstword $(wildcard source/main.cpp) main.cpp)

.PHONY: all clean
all: wii-game.dol

wii-game.dol: wii-game.elf

wii-game.elf: $(MAIN)
	$(CXX) $(MACHDEP) -O2 -Wall -Wextra -I"$(LIBOGC_INC)" $< \
		-L"$(LIBOGC_LIB)" -lwiiuse -lbte -logc -lm -o $@

clean:
	rm -f wii-game.elf wii-game.dol
