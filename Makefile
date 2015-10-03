all: tg

debug: tg-dbg

tg: interface.c algo.c audio.c tg.h
	gcc -Wall `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 fftw3f` -lm -O3 -fcx-limited-range -o tg interface.c algo.c audio.c

tg-dbg: interface.c algo.c audio.c tg.h
	gcc -Wall `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 fftw3f` -lm -O3 -fcx-limited-range -o tg-dbg -DDEBUG interface.c algo.c audio.c

clean:
	rm -f tg tg-dbg
