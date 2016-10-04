VERSION := $(shell cat version)

CC ?= gcc
BUILDDIR ?= build
PREFIX ?= /usr/local

PACKAGES := gtk+-2.0 gthread-2.0 portaudio-2.0 fftw3f
CFLAGS := -Wall -O3 -ffast-math -DVERSION='"$(VERSION)"' `pkg-config --cflags $(PACKAGES)`
LDFLAGS += -lm -lpthread `pkg-config --libs $(PACKAGES)`

CFILES := $(wildcard src/*.c)
HFILES := $(wildcard src/*.h)

ifeq ($(OS),Windows_NT)
	CFLAGS += -mwindows
	DEBUG_FLAGS := -mconsole
	EXT := .exe
	RESFILE := $(BUILDDIR)/tg-timer.res
else
	DEBUG_FLAGS :=
	EXT :=
	RESFILE :=
endif

ALLFILES := $(CFILES) $(HFILES) $(RESFILE) Makefile

COMPILE = $(CC) $(CFLAGS) -DPROGRAM_NAME='"$(1)"' $(2) -o $(BUILDDIR)/$(1)$(EXT) $(CFILES) $(RESFILE) $(LDFLAGS)

all: $(BUILDDIR)/tg$(EXT) $(BUILDDIR)/tg-lt$(EXT)
.PHONY: all

debug: $(BUILDDIR)/tg-dbg$(EXT) $(BUILDDIR)/tg-lt-dbg$(EXT)
.PHONY: debug

profile: $(BUILDDIR)/tg-prf$(EXT) $(BUILDDIR)/tg-lt-prf$(EXT)
.PHONY: profile

test: $(BUILDDIR)/tg-dbg$(EXT)
	$(BUILDDIR)/tg-dbg test
.PHONY: test

$(BUILDDIR)/tg-timer.res: packaging/tg-timer.rc icons/tg-timer.ico
	windres packaging/tg-timer.rc -O coff -o $(BUILDDIR)/tg-timer.res

$(BUILDDIR)/tg$(EXT): $(ALLFILES)
	$(call COMPILE,tg,)

$(BUILDDIR)/tg-lt$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt,-DLIGHT)

$(BUILDDIR)/tg-dbg$(EXT): $(ALLFILES)
	$(call COMPILE,tg-dbg,$(DEBUG_FLAGS) -ggdb -DDEBUG)

$(BUILDDIR)/tg-lt-dbg$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt-dbg,$(DEBUG_FLAGS) -ggdb -DDEBUG -DLIGHT)

$(BUILDDIR)/tg-prf$(EXT): $(ALLFILES)
	$(call COMPILE,tg-prf,-pg)

$(BUILDDIR)/tg-lt-prf$(EXT): $(ALLFILES)
	$(call COMPILE,tg-lt-prf,-DLIGHT -pg)

ICONSIZES := $(foreach SIZE, $(shell cat icons/sizes), $(SIZE)x$(SIZE))

$(ICONSIZES): %: icons/%/tg-timer.png
	install -D -m 0644 $< $(PREFIX)/share/icons/hicolor/$@/apps/tg-timer.png
.PHONY: $(ICONSIZES)

install: all $(ICONSIZES)
	install -D -m 0755 -s $(BUILDDIR)/tg$(EXT) $(PREFIX)/bin/tg-timer$(EXT)
	install -D -m 0755 -s $(BUILDDIR)/tg-lt$(EXT) $(PREFIX)/bin/tg-timer-lt$(EXT)
	install -D -m 0644 packaging/tg-timer.desktop $(PREFIX)/share/applications/tg-timer.desktop
	install -D -m 0644 packaging/tg-timer-lt.desktop $(PREFIX)/share/applications/tg-timer-lt.desktop
	install -D -m 0644 docs/tg-timer.1.gz $(PREFIX)/share/man/man1/tg-timer.1.gz
	ln -s tg-timer.1.gz $(PREFIX)/share/man/man1/tg-timer-lt.1.gz
.PHONY: install

clean:
	rm -rf $(BUILDDIR)/*
.PHONY: clean
