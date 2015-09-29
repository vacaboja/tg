all: tg

tg: interface.c algo.c tg.h
	gcc -Wall `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 fftw3f` -lm -O3 -fcx-limited-range -o tg interface.c algo.c audio.c
