all: tg tg-lt

debug: tg-dbg

profile: tg-prf tg-lt-prf

tg: interface.c algo.c audio.c tg.h
	gcc -Wall `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 fftw3f` -lm -O3 -fcx-limited-range -o tg interface.c algo.c audio.c

tg-lt: interface.c algo.c audio.c tg.h
	gcc -Wall `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 fftw3f` -lm -O3 -fcx-limited-range -o tg-lt -DLIGHT interface.c algo.c audio.c

tg-dbg: interface.c algo.c audio.c tg.h
	gcc -Wall `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 fftw3f` -lm -O3 -fcx-limited-range -o tg-dbg -DDEBUG interface.c algo.c audio.c

tg-prf: interface.c algo.c audio.c tg.h
	gcc -Wall `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 fftw3f` -lm -O3 -fcx-limited-range -o tg-prf -pg interface.c algo.c audio.c

tg-lt-prf: interface.c algo.c audio.c tg.h
	gcc -Wall `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 fftw3f` -lm -O3 -fcx-limited-range -o tg-lt-prf -pg -DLIGHT interface.c algo.c audio.c

clean:
	rm -f tg tg-lt tg-dbg tg-prf tg-lt-prf gmon.out
