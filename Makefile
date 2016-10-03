VERSION := 0.3.2

CC ?= gcc

PACKAGES := gtk+-2.0 gthread-2.0 portaudio-2.0 fftw3f
CFLAGS := -Wall -O3 -ffast-math -DVERSION='"$(VERSION)"' `pkg-config --cflags $(PACKAGES)`
LDFLAGS := -lm -lpthread `pkg-config --libs $(PACKAGES)`

CFILES := $(wildcard src/*.c)
HFILES := $(wildcard src/*.h)

BUILDDIR = build/

ifeq ($(OS),Windows_NT)
	CFLAGS += -mwindows
	DEBUG_FLAGS := -mconsole
	EXT := .exe
	RESFILE := $(BUILDDIR)tg-timer.res
else
	DEBUG_FLAGS :=
	EXT :=
	RESFILE :=
endif

ALLFILES := $(CFILES) $(HFILES) $(RESFILE) Makefile

COMPILE = $(CC) $(CFLAGS) -DPROGRAM_NAME='"$(1)"' $(2) -o $(BUILDDIR)$(1)$(EXT) $(CFILES) $(RESFILE) $(LDFLAGS)

all: $(BUILDDIR)tg$(EXT) $(BUILDDIR)tg-lt$(EXT)

debug: $(BUILDDIR)tg-dbg$(EXT) $(BUILDDIR)tg-lt-dbg$(EXT)

profile: $(BUILDDIR)tg-prf$(EXT) $(BUILDDIR)tg-lt-prf$(EXT)

test: $(BUILDDIR)tg-dbg$(EXT)
	$(BUILDDIR)tg-dbg test

$(BUILDDIR)tg-timer.res: tg-timer.rc icons/tg-timer.ico
	windres tg-timer.rc -O coff -o $(BUILDDIR)tg-timer.res

$(BUILDDIR)tg$(EXT): $(ALLFILES)
	$(call COMPILE,tg,)

$(BUILDDIR)tg-lt$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt,-DLIGHT)

$(BUILDDIR)tg-dbg$(EXT): $(ALLFILES)
	$(call COMPILE,tg-dbg,$(DEBUG_FLAGS) -ggdb -DDEBUG)

$(BUILDDIR)tg-lt-dbg$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt-dbg,$(DEBUG_FLAGS) -ggdb -DDEBUG -DLIGHT)

$(BUILDDIR)tg-prf$(EXT): $(ALLFILES)
	$(call COMPILE,tg-prf,-pg)

$(BUILDDIR)tg-lt-prf$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt-prf,-DLIGHT -pg)

clean:
	rm -f $(BUILDDIR)*
