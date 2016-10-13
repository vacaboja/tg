VERSION := $(shell cat version)

CC ?= gcc
BUILDDIR ?= build
PREFIX ?= /usr/local

ifneq ($(shell uname -s),Darwin)
	LDFLAGS += -Wl,--as-needed
endif

PACKAGES := gtk+-2.0 gthread-2.0 portaudio-2.0 fftw3f
CFLAGS += -Wall -O3 -ffast-math -DVERSION='"$(VERSION)"' `pkg-config --cflags $(PACKAGES)`
LDFLAGS += -lm -lpthread `pkg-config --libs $(PACKAGES)`

SRCDIR := src
CFILES := $(wildcard $(SRCDIR)/*.c)
HFILES := $(wildcard $(SRCDIR)/*.h)

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

all: $(BUILDDIR)/tg$(EXT) $(BUILDDIR)/tg-lt$(EXT)
.PHONY: all

debug: $(BUILDDIR)/tg-dbg$(EXT) $(BUILDDIR)/tg-lt-dbg$(EXT)
.PHONY: debug

profile: $(BUILDDIR)/tg-prf$(EXT) $(BUILDDIR)/tg-lt-prf$(EXT)
.PHONY: profile

test: $(BUILDDIR)/tg-dbg$(EXT)
	$(BUILDDIR)/tg-dbg test
.PHONY: test

$(BUILDDIR)/tg-timer.res: icons/tg-timer.rc icons/tg-timer.ico
	windres icons/tg-timer.rc -O coff -o $(BUILDDIR)/tg-timer.res

define TARGET
$(BUILDDIR)/$(1)_%.o: $(SRCDIR)/%.c $(HFILES)
	$(CC) -c $(CFLAGS) -DPROGRAM_NAME='"$(1)"' $(2) $$< -o $$@

$(BUILDDIR)/$(1)$(EXT): $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/$(1)_%.o,$(CFILES)) $(RESFILE)
	$(CC) -o $(BUILDDIR)/$(1)$(EXT) $$^ $(LDFLAGS)
ifeq ($(3),strip)
	strip $(BUILDDIR)/$(1)$(EXT)
endif
endef

$(eval $(call TARGET,tg,,strip))
$(eval $(call TARGET,tg-lt,-DLIGHT,strip))
$(eval $(call TARGET,tg-dbg,$(DEBUG_FLAGS) -ggdb -DDEBUG,))
$(eval $(call TARGET,tg-lt-dbg,$(DEBUG_FLAGS) -ggdb -DDEBUG -DLIGHT,))
$(eval $(call TARGET,tg-prf,-pg,))
$(eval $(call TARGET,tg-lt-prf,-DLIGHT -pg,))

ICONSIZES := $(foreach SIZE, $(shell cat icons/sizes), $(SIZE)x$(SIZE))

$(ICONSIZES): %: icons/%/tg-timer.png
	install -D -m 0644 $< $(PREFIX)/share/icons/hicolor/$@/apps/tg-timer.png
.PHONY: $(ICONSIZES)

install: all $(ICONSIZES)
	install -D -m 0755 $(BUILDDIR)/tg$(EXT) $(PREFIX)/bin/tg-timer$(EXT)
	install -D -m 0755 $(BUILDDIR)/tg-lt$(EXT) $(PREFIX)/bin/tg-timer-lt$(EXT)
	install -D -m 0644 icons/tg-timer.desktop $(PREFIX)/share/applications/tg-timer.desktop
	install -D -m 0644 icons/tg-timer-lt.desktop $(PREFIX)/share/applications/tg-timer-lt.desktop
	install -D -m 0644 docs/tg-timer.1.gz $(PREFIX)/share/man/man1/tg-timer.1.gz
	ln -s tg-timer.1.gz $(PREFIX)/share/man/man1/tg-timer-lt.1.gz
.PHONY: install

clean:
	rm -rf $(BUILDDIR)/*
.PHONY: clean
