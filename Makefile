CFLAGS = -Wall -O3 -fcx-limited-range `pkg-config --cflags gtk+-2.0 portaudio-2.0 fftw3f`
LDFLAGS = -lm `pkg-config --libs gtk+-2.0 portaudio-2.0 fftw3f`

ifeq ($(OS),Windows_NT)
	CFLAGS += -mwindows
	EXT = .exe
else
	EXT =
endif

all: tg$(EXT) tg-lt$(EXT)

debug: tg-dbg$(EXT)

profile: tg-prf$(EXT) tg-lt-prf$(EXT)

tg$(EXT): interface.c algo.c audio.c tg.h
	gcc $(CFLAGS) -o tg$(EXT) interface.c algo.c audio.c $(LDFLAGS)

tg-lt$(EXT): interface.c algo.c audio.c tg.h
	gcc $(CFLAGS) -DLIGHT -o tg-lt$(EXT) interface.c algo.c audio.c $(LDFLAGS)

tg-dbg$(EXT): interface.c algo.c audio.c tg.h
	gcc $(CFLAGS) -ggdb -DDEBUG -o tg-dbg$(EXT) interface.c algo.c audio.c $(LDFLAGS)

tg-prf$(EXT): interface.c algo.c audio.c tg.h
	gcc $(CFLAGS) -pg -o tg-prf$(EXT) interface.c algo.c audio.c $(LDFLAGS)

tg-lt-prf$(EXT): interface.c algo.c audio.c tg.h
	gcc $(CFLAGS) -DLIGHT -pg -o tg-lt-prf$(EXT) interface.c algo.c audio.c $(LDFLAGS)

clean:
	rm -f tg$(EXT) tg-lt$(EXT) tg-dbg$(EXT) tg-prf$(EXT) tg-lt-prf$(EXT) gmon.out
