VERSION = 0.3.2

CC ?= gcc

PACKAGES = gtk+-2.0 gthread-2.0 portaudio-2.0 fftw3f
CFLAGS = -Wall -O3 -ffast-math -DVERSION='"$(VERSION)"' `pkg-config --cflags $(PACKAGES)`
LDFLAGS = -lm -lpthread `pkg-config --libs $(PACKAGES)`

CFILES = interface.c algo.c audio.c config.c
HFILES = tg.h
ALLFILES = $(CFILES) $(HFILES) Makefile

ifeq ($(OS),Windows_NT)
	CFLAGS += -mwindows
	DEBUG_FLAGS = -mconsole
	EXT = .exe
else
	DEBUG_FLAGS =
	EXT =
endif

COMPILE = $(CC) $(CFLAGS) -DPROGRAM_NAME='"$(1)"' $(2) -o $(1)$(EXT) $(CFILES) $(LDFLAGS)

all: tg$(EXT) tg-lt$(EXT)

debug: tg-dbg$(EXT) tg-lt-dbg$(EXT)

profile: tg-prf$(EXT) tg-lt-prf$(EXT)

test: tg-dbg$(EXT)
	./tg-dbg test

tg$(EXT): $(ALLFILES)
	$(call COMPILE,tg,)

tg-lt$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt,-DLIGHT)

tg-dbg$(EXT): $(ALLFILES)
	$(call COMPILE,tg-dbg,$(DEBUG_FLAGS) -ggdb -DDEBUG)

tg-lt-dbg$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt-dbg,$(DEBUG_FLAGS) -ggdb -DDEBUG -DLIGHT)

tg-prf$(EXT): $(ALLFILES)
	$(call COMPILE,tg-prf,-pg)

tg-lt-prf$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt-prf,-DLIGHT -pg)

clean:
	rm -f tg$(EXT) tg-lt$(EXT) tg-dbg$(EXT) tg-lt-dbg$(EXT) tg-prf$(EXT) tg-lt-prf$(EXT) gmon.out perf.data*
