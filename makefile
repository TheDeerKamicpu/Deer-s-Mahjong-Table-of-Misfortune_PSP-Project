TARGET = deersmahjong

OBJS = main.o game_core.o texture.o audio.o

CFLAGS = -O2 -G0 -Wall

CXXFLAGS = $(CFLAGS) \
	-fno-exceptions \
	-fno-rtti \
	-std=gnu++11

ASFLAGS = $(CFLAGS)

LIBS = \
	-lpspdebug \
	-lpspctrl \
	-lpspdisplay \
	-lpspaudio \
	-lpspkernel \
	-lstdc++ \
	-lm

EXTRA_TARGETS = EBOOT.PBP

PSP_EBOOT_TITLE = The Deer’s Mahjong Table of Misfortune

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak


package: EBOOT.PBP
	rm -rf dist/DEERSMAHJONG
	mkdir -p dist/DEERSMAHJONG
	cp EBOOT.PBP dist/DEERSMAHJONG/EBOOT.PBP
	cp -r assets dist/DEERSMAHJONG/assets
	@echo ""
	@echo "PACKAGE READY:"
	@echo "dist/DEERSMAHJONG/"
