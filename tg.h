/*
    tg
    Copyright (C) 2015 Marcello Mamino

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License version 2 as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <stdint.h>

#define FILTER_CUTOFF 3000

#ifdef LIGHT

#define NSTEPS 4
#define FIRST_STEP 0
#define PA_SAMPLE_RATE 44100
#define PA_BUFF_SIZE (PA_SAMPLE_RATE << (NSTEPS + FIRST_STEP + 1))

#else

#define NSTEPS 4
#define FIRST_STEP 1
#define PA_SAMPLE_RATE 44100
#define PA_BUFF_SIZE (PA_SAMPLE_RATE << (NSTEPS + FIRST_STEP))

#endif

#define OUTPUT_FONT 40
#define OUTPUT_WINDOW_HEIGHT 70

#define POSITIVE_SPAN 10
#define NEGATIVE_SPAN 25

#define EVENTS_COUNT 10000
#define EVENTS_MAX 100
#define PAPERSTRIP_ZOOM 10

#define MIN_BPH 12000
#define MAX_BPH 72000
#define DEFAULT_BPH 21600
#define MIN_LA 10
#define MAX_LA 90
#define DEFAULT_LA 52

#define PRESET_BPH { 12000, 14400, 18000, 19800, 21600, 25200, 28800, 36000, 43200, 72000, 0 };

#define INTERFACE_FONT "VeraMono.ttf"

#ifdef DEBUG
#define debug(...) print_debug(__VA_ARGS__)
#else
#define debug(...)
#endif

/* algo.c */
struct filter {
	double a0,a1,a2,b1,b2;
};

struct processing_buffers {
	int sample_rate;
	int sample_count;
	float *samples, *samples_sc, *waveform, *waveform_sc, *tic_wf, *tic_c;
	fftwf_complex *fft, *sc_fft, *tic_fft;
	fftwf_plan plan_a, plan_b, plan_c, plan_d, plan_e, plan_f;
	struct filter *hpf, *lpf;
	double period,sigma,be,waveform_max,phase,tic_pulse,toc_pulse;
	int tic,toc;
	int ready;
	uint64_t timestamp, last_tic, last_toc, events_from;
	uint64_t *events;
#ifdef DEBUG
	float *debug;
#endif
};

void setup_buffers(struct processing_buffers *b);
struct processing_buffers *pb_clone(struct processing_buffers *p);
void pb_destroy_clone(struct processing_buffers *p);
void process(struct processing_buffers *p, int bph);

/* audio.c */
int start_portaudio(int *nominal_sample_rate, double *real_sample_rate);
int analyze_pa_data(struct processing_buffers *p, int bph, uint64_t events_from);

/* interface.c */
void print_debug(char *format,...);
void error(char *format,...);
