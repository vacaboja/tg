VERSION = 0.1.0

CC = gcc

CFLAGS = -Wall -O3 -DVERSION='"$(VERSION)"' `pkg-config --cflags gtk+-2.0 portaudio-2.0 fftw3f`
LDFLAGS = -lm `pkg-config --libs gtk+-2.0 portaudio-2.0 fftw3f`

CFILES = interface.c algo.c audio.c
HFILES = tg.h

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

tg$(EXT): $(CFILES) $(HFILES)
	$(call COMPILE,tg,)

tg-lt$(EXT): $(CFILES) $(HFILES)
	$(call COMPILE,tg-lt,-DLIGHT)

tg-dbg$(EXT): $(CFILES) $(HFILES)
	$(call COMPILE,tg-dbg,-ggdb -DDEBUG)

tg-prf$(EXT): $(CFILES) $(HFILES)
	$(call COMPILE,tg-prf,-pg)

tg-lt-prf$(EXT): $(CFILES) $(HFILES)
	$(call COMPILE,tg-prf,-DLIGHT -pg)

clean:
	rm -f tg$(EXT) tg-lt$(EXT) tg-dbg$(EXT) tg-prf$(EXT) tg-lt-prf$(EXT) gmon.out
