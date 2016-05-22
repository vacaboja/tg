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
#include <stdint.h>
#include <complex.h>
#include <fftw3.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <pthread.h>

#define CONFIG_FILE_NAME "tg-timer.ini"

#define FILTER_CUTOFF 3000

#define CAL_DATA_SIZE 1000

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
#define PAPERSTRIP_MARGIN .2

#define MIN_BPH 12000
#define MAX_BPH 72000
#define DEFAULT_BPH 21600
#define MIN_LA 10
#define MAX_LA 90
#define DEFAULT_LA 52
#define MIN_CAL -99.9
#define MAX_CAL 99.9

#define PRESET_BPH { 12000, 14400, 18000, 19800, 21600, 25200, 28800, 36000, 43200, 72000, 0 };

#ifdef DEBUG
#define debug(...) print_debug(__VA_ARGS__)
#else
#define debug(...)
#endif

/* algo.c */
struct processing_buffers {
	int sample_rate;
	int sample_count;
	float *samples, *samples_sc, *waveform, *waveform_sc, *tic_wf, *slice_wf, *tic_c;
	fftwf_complex *fft, *sc_fft, *tic_fft, *slice_fft;
	fftwf_plan plan_a, plan_b, plan_c, plan_d, plan_e, plan_f, plan_g;
	struct filter *hpf, *lpf;
	double period,sigma,be,waveform_max,phase,tic_pulse,toc_pulse;
	double cal_phase;
	int waveform_max_i;
	int tic,toc;
	int ready;
	uint64_t timestamp, last_tic, last_toc, events_from;
	uint64_t *events;
#ifdef DEBUG
	float *debug;
#endif
};

struct calibration_data {
	int wp;
	int size;
	int state;
	double calibration;
	uint64_t start_time;
	double *times;
	double *phases;
};

void setup_buffers(struct processing_buffers *b);
struct processing_buffers *pb_clone(struct processing_buffers *p);
void pb_destroy_clone(struct processing_buffers *p);
void process(struct processing_buffers *p, int bph);
int test_cal(struct processing_buffers *p);
int process_cal(struct processing_buffers *p, struct calibration_data *cd);

/* audio.c */
struct processing_data {
	struct processing_buffers *buffers;
	uint64_t last_tic;
};

int start_portaudio(int *nominal_sample_rate, double *real_sample_rate);
int terminate_portaudio();
int analyze_pa_data(struct processing_data *pd, int bph, uint64_t events_from);
int analyze_pa_data_cal(struct processing_data *pd, struct calibration_data *cd);

/* interface.c */
struct main_window {
	GtkWidget *window;
	GtkWidget *output_drawing_area;
	GtkWidget *tic_drawing_area;
	GtkWidget *toc_drawing_area;
	GtkWidget *period_drawing_area;
	GtkWidget *paperstrip_drawing_area;
	GtkWidget *cal_spin_button;
#ifdef DEBUG
	GtkWidget *debug_drawing_area;
#endif

	pthread_t computing_thread;
	pthread_mutex_t recompute_mutex;
	pthread_cond_t recompute_cond;
	int recompute;
	guint computer_kicker;

	int calibrate;
	int calibrating;
	uint64_t cal_time;
	int cal_updated;

	struct processing_data *pdata;
	struct processing_buffers *old;
	struct calibration_data *cdata;
	int is_old;

	int bph;
	int guessed_bph;
	int last_bph;
	double la;
	double cal;
	double sample_rate;
	int nominal_sr;

	uint64_t *events;
	int events_wp;
	uint64_t events_from;
	double trace_centering;

	int signal;

	GKeyFile *config_file;
	gchar *config_file_name;
};

#ifdef DEBUG
extern int testing;
#endif

void print_debug(char *format,...);
void error(char *format,...);

// config.c
void load_config(struct main_window *w);
void save_config(struct main_window *w);
