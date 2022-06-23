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

#ifndef SRC_TG_H_
#define SRC_TG_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <fftw3.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include "audio.h"

#ifdef __CYGWIN__
#define _WIN32
#endif

#define CONFIG_FILE_NAME "tg-timer.ini"

#define FILTER_CUTOFF 3000

#define CAL_DATA_SIZE 900

#define FIRST_STEP 1
#define FIRST_STEP_LIGHT 0

#define NSTEPS 4
#define PA_SAMPLE_RATE 44100u
#define MAX_PA_SAMPLE_RATE 48000u
#define PA_BUFF_SIZE (MAX_PA_SAMPLE_RATE << (NSTEPS + FIRST_STEP))

#define OUTPUT_FONT 40
#define OUTPUT_WINDOW_HEIGHT 70

#define POSITIVE_SPAN 10
#define NEGATIVE_SPAN 25

#define EVENTS_COUNT 10000
#define EVENTS_MAX 100
#define PAPERSTRIP_ZOOM 10
#define PAPERSTRIP_ZOOM_CAL 100
#define PAPERSTRIP_MARGIN .2

#define MIN_BPH 12000
#define MAX_BPH 72000
#define DEFAULT_BPH 21600
#define MIN_LA 10 // deg
#define MAX_LA 90 // deg
#define DEFAULT_LA 52 // deg
#define MIN_CAL -1000 // 0.1 s/d
#define MAX_CAL 1000 // 0.1 s/d

#define PRESET_BPH { 12000, 14400, 17280, 18000, 19800, 21600, 25200, 28800, 36000, 43200, 72000, 0 };

#ifdef DEBUG
#define debug(...) print_debug(__VA_ARGS__)
#else
#define debug(...) {}
#endif

#define UNUSED(X) (void)(X)

#define TIC 1
#define TOC -1
#define BAD_EVENT_TIME -1
#define NULL_EVENT_TIME 0

/* algo.c */
struct processing_buffers {
	int sample_rate;
	int sample_count;
	float *samples, *samples_sc, *waveform, *waveform_sc, *tic_wf, *slice_wf, *tic_c;
	fftwf_complex *fft, *sc_fft, *tic_fft, *slice_fft;
	fftwf_plan plan_a, plan_b, plan_c, plan_d, plan_e, plan_f, plan_g;
	struct filter *hpf, *lpf;
	double period,sigma,be,waveform_max,phase,tic_pulse,toc_pulse,amp;
	double cal_phase;
	int waveform_max_i;
	int tic,toc;
	int ready;
	uint64_t timestamp, last_tic, last_toc, events_from;
	uint64_t *events;
#ifdef DEBUG
	int debug_size;
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
	uint64_t *events;
};

void setup_buffers(struct processing_buffers *b, double lpfCutoff, double hpfCutoff);
void pb_destroy(struct processing_buffers *b);
struct processing_buffers *pb_clone(struct processing_buffers *p);
void pb_destroy_clone(struct processing_buffers *p);
void process(struct processing_buffers *p, int bph, double la, int light);
void setup_cal_data(struct calibration_data *cd);
void cal_data_destroy(struct calibration_data *cd);
int test_cal(struct processing_buffers *p);
int process_cal(struct processing_buffers *p, struct calibration_data *cd);
int absEventTime(int eventTime);
int event_is_TIC_or_TOC(int eventTime);
void setFilter(gboolean bLpf, double freq);
void pb_setFilter(struct processing_buffers *b, gboolean bLpf, double freq);

/* audio.c   moved to audio.h
struct processing_data {
	struct processing_buffers *buffers;
	uint64_t last_tic;
	int is_light;
};

int start_portaudio(int *nominal_sample_rate, double *real_sample_rate);
int terminate_portaudio();
uint64_t get_timestamp(int light);
int analyze_pa_data(struct processing_data *pd, int bph, double la, uint64_t events_from);
int analyze_pa_data_cal(struct processing_data *pd, struct calibration_data *cd);
void set_audio_light(bool light);
*/


/* computer.c */
struct snapshot {
	struct processing_buffers *pb;
	int is_old;
	uint64_t timestamp;
	int is_light;

	int nominal_sr;
	int calibrate;
	int bph;
	double la; // deg
	int cal; // 0.1 s/d

	int events_count;
	uint64_t *events; // used in cal+timegrapher mode
	int events_wp; // used in cal+timegrapher mode
	uint64_t events_from; // used only in timegrapher mode

	int signal;

	int cal_state;
	int cal_percent;
	int cal_result; // 0.1 s/d

	// data dependent on bph, la, cal
	double sample_rate;
	int guessed_bph;
	double rate;
	double be;
	double amp;

	double trace_centering;
};

struct computer {
	pthread_t thread;
	pthread_mutex_t mutex;
	pthread_cond_t cond;

// controlled by interface
	int recompute;
	int calibrate;
	int bph;
	double la; // deg
	int clear_trace;
	void (*callback)(void *);
	void *callback_data;

	struct processing_data *pdata;
	struct calibration_data *cdata;

	struct snapshot *actv;
	struct snapshot *curr;
};

struct snapshot *snapshot_clone(struct snapshot *s);
void snapshot_destroy(struct snapshot *s);
void computer_destroy(struct computer *c);
struct computer *start_computer(int nominal_sr, int bph, double la, int cal, int light, double lpfCutoff, double hpfCutoff);
void lock_computer(struct computer *c);
void unlock_computer(struct computer *c);
void compute_results(struct snapshot *s);
gboolean computer_setFilter(struct computer *c, gboolean bLpf, double freq);

/* output_panel.c */
struct output_panel {
	GtkWidget *panel;

	GtkWidget *output_drawing_area;
	GtkWidget *tic_drawing_area;
	GtkWidget *toc_drawing_area;
	GtkWidget *period_drawing_area;
	GtkWidget *paperstrip_drawing_area;
	GtkWidget *clear_button;
#ifdef DEBUG
	GtkWidget *debug_drawing_area;
#endif
	struct computer *computer;
	struct snapshot *snst;
};

void initialize_palette();
struct output_panel *init_output_panel(struct computer *comp, struct snapshot *snst, int border);
void redraw_op(struct output_panel *op);
void op_set_snapshot(struct output_panel *op, struct snapshot *snst);
void op_set_border(struct output_panel *op, int i);
void op_destroy(struct output_panel *op);

/* interface.c */

#define POSITIONS 6

struct main_window {
	GtkApplication *app;

	GtkWidget *window;
	GtkWidget *bph_combo_box;
	GtkWidget *la_spin_button;
	GtkWidget *cal_spin_button;
	GtkWidget *snapshot_button;
	GtkWidget *snapshot_POS_button[POSITIONS];
	GtkWidget *snapshot_name;
	GtkWidget *snapshot_name_entry;
	GtkWidget *cal_button;
	GtkWidget *notebook;
	GtkWidget *save_item;
	GtkWidget *save_all_item;
	GtkWidget *close_all_item;
	struct output_panel *active_panel;

	struct computer *computer;
	struct snapshot *active_snapshot;
	int computer_timeout;

	int is_light;
	int zombie;
	int controls_active;
	int calibrate;
	int bph;
	double la; // deg
	int cal; // 0.1 s/d
	int nominal_sr;

	GKeyFile *config_file;
	gchar *config_file_name;
	struct conf_data *conf_data;

	guint kick_timeout;
	guint save_timeout;

	char * audioInputStr;
	char * audioInterfaceStr;

	double lpfCutoff;
	double hpfCutoff;

};

extern int preset_bph[];

#ifdef DEBUG
extern int testing;
#endif

void print_debug(char *format,...);
void error(char *format,...);
gboolean interface_setFilter(struct main_window *w, gboolean bLpf, double freq);

//settings.c
void show_preferences(GtkButton *button, struct main_window *w);
#define DEFAULT_AUDIOINPUTSTRING "Default Audio Input"
#define DEFAULT_AUDIOINTERFACESTRING "Default Audio Interface"
#define DEFAULT_AUDIORATESTRING  "Default Sample Rate"
#define USE_DEVICE_DEFAULT_AUDIORATE 0    //passes control of the sample rate to the device itself

/* config.c */
#define CONFIG_FIELDS(OP) \
	OP(bph, bph, int) \
	OP(lift_angle, la, double) \
	OP(calibration, cal, int) \
	OP(light_algorithm, is_light, int) \
	OP(lpfCutoff, lpfCutoff, double) \
	OP(hpfCutoff, hpfCutoff, double)

struct conf_data {
#define DEF(NAME,PLACE,TYPE) TYPE PLACE;
	CONFIG_FIELDS(DEF)
};

void load_config(struct main_window *w);
void save_config(struct main_window *w);
void save_on_change(struct main_window *w);
void close_config(struct main_window *w);

/* serializer.c */
int write_file(FILE *f, struct snapshot **s, char **names, uint64_t cnt);
int read_file(FILE *f, struct snapshot ***s, char ***names, uint64_t *cnt);

#endif /* SRC_TG_H_ */
