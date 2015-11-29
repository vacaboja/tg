VERSION = 0.2.1

CC = gcc

CFLAGS = -Wall -O3 -DVERSION='"$(VERSION)"' `pkg-config --cflags gtk+-2.0 portaudio-2.0 fftw3f`
LDFLAGS = -lm `pkg-config --libs gtk+-2.0 freetype2 portaudio-2.0 fftw3f`

CFILES = interface.c algo.c audio.c
HFILES = tg.h
ALLFILES = $(CFILES) $(HFILES) Makefile

ifeq ($(OS),Windows_NT)
	CFLAGS += -mwindows
	EXT = .exe
else
	EXT =
endif

COMPILE = $(CC) $(CFLAGS) -DPROGRAM_NAME='"$(1)"' $(2) -o $(1)$(EXT) $(CFILES) $(LDFLAGS)

all: tg$(EXT) tg-lt$(EXT)

debug: tg-dbg$(EXT)

profile: tg-prf$(EXT) tg-lt-prf$(EXT)

tg$(EXT): $(ALLFILES)
	$(call COMPILE,tg,)

tg-lt$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt,-DLIGHT)

tg-dbg$(EXT): $(ALLFILES)
	$(call COMPILE,tg-dbg,-ggdb -DDEBUG)

tg-prf$(EXT): $(ALLFILES)
	$(call COMPILE,tg-prf,-pg)

tg-lt-prf$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt-prf,-DLIGHT -pg)

clean:
	rm -f tg$(EXT) tg-lt$(EXT) tg-dbg$(EXT) tg-prf$(EXT) tg-lt-prf$(EXT) gmon.out
