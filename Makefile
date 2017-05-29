VERSION := $(shell cat version)

CC ?= gcc
BUILDDIR ?= build
PREFIX ?= /usr/local

ifneq ($(shell uname -s),Darwin)
	LDFLAGS += -Wl,--as-needed
endif

PACKAGES := gtk+-3.0 portaudio-2.0 fftw3f
CFLAGS += -Wall -ffast-math -DVERSION='"$(VERSION)"' `pkg-config --cflags $(PACKAGES)`
LDFLAGS += -lm -lpthread `pkg-config --libs $(PACKAGES)`

SRCDIR := src
CFILES := $(wildcard $(SRCDIR)/*.c)
HFILES := $(wildcard $(SRCDIR)/*.h)

ifeq ($(OS),Windows_NT)
	LDFLAGS += -mwindows
	DEBUG_LDFLAGS := -mconsole
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

valgrind: $(BUILDDIR)/tg-vlg$(EXT)
	valgrind --leak-check=full -v --num-callers=99 --suppressions=.valgrind.supp $(BUILDDIR)/tg-vlg$(EXT)
.PHONY: debug

$(BUILDDIR)/tg-timer.res: icons/tg-timer.rc icons/tg-timer.ico
	windres icons/tg-timer.rc -O coff -o $(BUILDDIR)/tg-timer.res

define TARGET
$(BUILDDIR)/$(1)_%.o: $(SRCDIR)/%.c $(HFILES)
	$(CC) -c $(CFLAGS) -DPROGRAM_NAME='"$(1)"' $(2) $$< -o $$@

$(BUILDDIR)/$(1)$(EXT): $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/$(1)_%.o,$(CFILES)) $(RESFILE)
	$(CC) -o $(BUILDDIR)/$(1)$(EXT) $$^ $(LDFLAGS) $(3)
ifeq ($(4),strip)
	strip $(BUILDDIR)/$(1)$(EXT)
endif
endef

$(eval $(call TARGET,tg,-O3,,strip))
$(eval $(call TARGET,tg-lt,-O3 -DLIGHT,,strip))
$(eval $(call TARGET,tg-dbg,-O3 -ggdb -DDEBUG,$(DEBUG_LDFLAGS),))
$(eval $(call TARGET,tg-lt-dbg,-O3 -ggdb -DDEBUG -DLIGHT,$(DEBUG_LDFLAGS),))
$(eval $(call TARGET,tg-prf,-O3 -pg,,))
$(eval $(call TARGET,tg-lt-prf,-O3 -DLIGHT -pg,,))
$(eval $(call TARGET,tg-vlg,-O1 -g,,))
$(eval $(call TARGET,tg-vlg-lt,-O1 -g -DLIGHT,,))

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
