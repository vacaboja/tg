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
#include <stdbool.h>
#include <stdint.h>
#include <stdbool.h>
#include <complex.h>
#include <fftw3.h>
#include <stdarg.h>
#include <gtk/gtk.h>
#include <pthread.h>

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

#define OUTPUT_FONT 40
#define OUTPUT_WINDOW_HEIGHT 70

#define POSITIVE_SPAN 10
#define NEGATIVE_SPAN 25

#define EVENTS_COUNT 10000
#define EVENTS_MAX 100
#define PAPERSTRIP_ZOOM 10
#define PAPERSTRIP_ZOOM_CAL 100
#define PAPERSTRIP_MARGIN .2

#define MIN_BPH 8100
#define TYP_BPH 12000
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
#define BIT(n) (1u << (n))
#define BITMASK(n) ((1u << (n)) - 1u)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* algo.c */
struct processing_buffers {
	int sample_rate;
	int sample_count;
	float *samples, *samples_sc, *waveform, *waveform_sc, *tic_wf, *slice_wf, *tic_c;
	fftwf_complex *fft, *sc_fft, *tic_fft, *slice_fft;
	fftwf_plan plan_a, plan_b, plan_c, plan_d, plan_e, plan_f, plan_g;
	struct filter *lpf;
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

struct filter {
	double a0,a1,a2,b1,b2;
};

void setup_buffers(struct processing_buffers *b);
void pb_destroy(struct processing_buffers *b);
struct processing_buffers *pb_clone(struct processing_buffers *p);
void pb_destroy_clone(struct processing_buffers *p);
void process(struct processing_buffers *p, int bph, double la, int light);
void setup_cal_data(struct calibration_data *cd);
void cal_data_destroy(struct calibration_data *cd);
int test_cal(struct processing_buffers *p);
int process_cal(struct processing_buffers *p, struct calibration_data *cd);
void make_hp(struct filter *f, double freq);

/* audio.c */
struct processing_data {
	struct processing_buffers *buffers;
	uint64_t last_tic;
	int last_step;	//!< Guess of step (buffers index) to try first, based on last iteration
	int is_light;
};

#define AUDIO_RATES       {22050,       44100,      48000,    96000,     192000 }
#define AUDIO_RATE_LABELS {"22.05 kHz", "44.1 kHz", "48 kHz", "96 kHz", "192 kHz" }
#define NUM_AUDIO_RATES ARRAY_SIZE((int[])AUDIO_RATES)

int start_portaudio(int *nominal_sample_rate, double *real_sample_rate, bool light);
int terminate_portaudio();
uint64_t get_timestamp();
void fill_buffers(struct processing_buffers *ps);
bool analyze_pa_data(struct processing_data *pd, int step, int bph, double la, uint64_t events_from);
int analyze_pa_data_cal(struct processing_data *pd, struct calibration_data *cd);
void set_audio_light(bool light);
struct audio_device {
	const char* name;      //!< Name of device from port audio
	bool        good;      //!< Is this suitable or not?  E.g., playback only.
	bool	    isdefault; //!< This is the default device;
	unsigned    rates;     //!< Bitmask of allowed rates from AUDIO_RATES

};
int get_audio_devices(const struct audio_device **devices);
int get_audio_device(void);
int set_audio_device(int device, int *nominal_sr, double *real_sr, bool light);

/* computer.c */
struct display;

struct snapshot {
	struct processing_buffers *pb;
	int is_old;
	uint64_t timestamp;
	int is_light;

	int nominal_sr; // W/O calibration, but does include light mode decimation
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
	double sample_rate; // Includes calibration
	int guessed_bph;
	double rate;
	double be;
	double amp;

	// State related to displaying the snapshot, not generated by computer
	struct display *d;
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
struct computer *start_computer(int nominal_sr, int bph, double la, int cal, int light);
void lock_computer(struct computer *c);
void unlock_computer(struct computer *c);
void compute_results(struct snapshot *s);

/* output_panel.c */
/* Snapshot display parameters, e.g. scale, centering. */
struct display {
	// Scaling factor for each beat.  1 means the chart is 1 beat wide, 0.5
	// means half a beat, etc.
	double beat_scale;
	/* Time of point used to anchor the paperstrip.  Each paperstrip point's position is
	 * relative to the previous point.  This point is the one with an absolute position that
	 * is kept the same, so that all the dots do not shift side to side as the scroll.  */
	uint64_t anchor_time;
	// Phase offset of point at anchor_time
	double anchor_offset;
};

struct output_panel {
	GtkWidget *panel;

	GtkWidget *output_drawing_area;
	GtkWidget *displays;
	GtkWidget *waveforms_box;
	GtkWidget *tic_drawing_area;
	GtkWidget *toc_drawing_area;
	GtkWidget *period_drawing_area;
	GtkWidget *paperstrip_box;
	GtkWidget *paperstrip_drawing_area;
	GtkWidget *clear_button;
	GtkWidget *left_button;
	GtkWidget *right_button;
	GtkWidget *zoom_button;
	GtkWidget *zoom_orig_button;
#ifdef DEBUG
	GtkWidget *debug_drawing_area;
#endif
	bool vertical_layout;

	struct computer *computer;
	struct snapshot *snst;
};

void initialize_palette();
struct output_panel *init_output_panel(struct computer *comp, struct snapshot *snst, int border, bool vertical_layout);
void set_panel_layout(struct output_panel *op, bool vertical);
void redraw_op(struct output_panel *op);
void op_set_snapshot(struct output_panel *op, struct snapshot *snst);
void op_set_border(struct output_panel *op, int i);
void op_destroy(struct output_panel *op);

/* interface.c */
struct main_window {
	GtkApplication *app;

	GtkWidget *window;
	GtkWidget *bph_combo_box;
	GtkWidget *la_spin_button;
	GtkWidget *cal_spin_button;
	GtkWidget *snapshot_button;
	GtkWidget *snapshot_name;
	GtkWidget *snapshot_name_entry;
	GtkWidget *cal_button;
	GtkWidget *notebook;
	GtkWidget *save_item;
	GtkWidget *save_all_item;
	GtkWidget *close_all_item;

	/* Audio Setup dialog */
	GtkWidget *audio_setup;
	GtkComboBox *device_list;
	GtkComboBox *rate_list;

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
	int nominal_sr;	// requested audio device rate

	bool vertical_layout;

	GKeyFile *config_file;
	gchar *config_file_name;
	struct conf_data *conf_data;

	guint kick_timeout;
	guint save_timeout;
};

extern int preset_bph[];

#ifdef DEBUG
extern int testing;
#endif

void print_debug(char *format,...);
void error(char *format,...);

/* config.c */
#define CONFIG_FIELDS(OP) \
	OP(bph, bph, int) \
	OP(lift_angle, la, double) \
	OP(calibration, cal, int) \
	OP(light_algorithm, is_light, int) \
	OP(vertical_paperstrip, vertical_layout, bool)

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
