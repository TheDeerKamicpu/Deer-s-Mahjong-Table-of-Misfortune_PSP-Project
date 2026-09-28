TARGET = deersmahjong

OBJS = main.o game_core.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti -std=gnu++11
ASFLAGS = $(CFLAGS)

LIBS = -lpspdebug -lpspctrl -lpspdisplay -lpspkernel -lstdc++ -lm

EXTRA_TARGETS = EBOOT.PBP

PSP_EBOOT_TITLE = The Deers MahJong Table of Misfortune

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak