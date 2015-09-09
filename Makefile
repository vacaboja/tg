all: tg

tg: tg.c
	gcc `pkg-config --libs --cflags gtk+-2.0 portaudio-2.0 sndfile fftw3f` -lm tg.c -O3 -fcx-limited-range -o tg
