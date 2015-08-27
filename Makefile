all: tg

tg: tg.c
	gcc -lportaudio -lsndfile -lfftw3f -lm tg.c -O3 -o tg
