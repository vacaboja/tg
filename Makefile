VERSION = 0.2.4-test2

CC = gcc

OPENMP = yes

PACKAGES = gtk+-2.0 gthread-2.0 portaudio-2.0 fftw3f hwloc
CFLAGS = -Wall -O3 -ffast-math -DVERSION='"$(VERSION)"' `pkg-config --cflags $(PACKAGES)`
LDFLAGS = -lm -lpthread `pkg-config --libs $(PACKAGES)`

CFILES = interface.c algo.c audio.c
HFILES = tg.h
ALLFILES = $(CFILES) $(HFILES) Makefile

ifeq ($(OPENMP),yes)
	CFLAGS += -fopenmp
endif

ifeq ($(OS),Windows_NT)
	CFLAGS += -mwindows
	EXT = .exe
else
ifeq ($(OPENMP),yes)
	LDFLAGS += -lfftw3f_omp
else
	LDFLAGS += -lfftw3f_threads
endif
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
	rm -f tg$(EXT) tg-lt$(EXT) tg-dbg$(EXT) tg-prf$(EXT) tg-lt-prf$(EXT) gmon.out perf.data*
