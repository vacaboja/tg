#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <stdint.h>
#include <portaudio.h>
#include <stdarg.h>
#include <gtk/gtk.h>

#include <assert.h>

#define FILTER_CUTOFF 3000

#define NSTEPS 4
#define FIRST_STEP 1
#define PA_SAMPLE_RATE 44100
#define PA_BUFF_SIZE (PA_SAMPLE_RATE << (NSTEPS + FIRST_STEP))

#define FPS 2
#define AMPLITUDE_WIDTH .07

#define OUTPUT_FONT 50
#define OUTPUT_WINDOW_HEIGHT 80

#define POSITIVE_SPAN 10
#define NEGATIVE_SPAN 25

#define EVENTS_COUNT 10000

#define MIN_BPH 12000
#define MAX_BPH 36000
#define DEFAULT_BPH 21600
#define MIN_LA 10
#define MAX_LA 90
#define DEFAULT_LA 52

int preset_bph[] = { 12000, 14400, 18000, 19800, 21600, 25200, 28800, 36000, 0 };

volatile float pa_buffers[2][PA_BUFF_SIZE];
volatile int write_pointer = 0;
volatile uint64_t timestamp = 0;

void debug(char *format,...)
{
	va_list args;
	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);
}

void error(char *format,...)
{
	va_list args;
	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);
}

int paudio_callback(const void *input_buffer,
			void *output_buffer,
			unsigned long frame_count,
			const PaStreamCallbackTimeInfo *time_info,
			PaStreamCallbackFlags status_flags,
			void *data)
{
	unsigned long i;
	for(i=0; i < frame_count; i++) {
		pa_buffers[0][write_pointer] = ((float *)input_buffer)[2*i];
		pa_buffers[1][write_pointer] = ((float *)input_buffer)[2*i + 1];
		/*
		if(timestamp % 4410 > 4000) {
			pa_buffers[0][write_pointer] = 1; //timestamp % 2;
			pa_buffers[1][write_pointer] = 1; //timestamp % 2;
		} else {
			pa_buffers[0][write_pointer] = 0;
			pa_buffers[1][write_pointer] = 0;
		}
		*/
		if(write_pointer < PA_BUFF_SIZE - 1) write_pointer++;
		else write_pointer = 0;
		timestamp++;
	}
	return 0;
}

int start_portaudio()
{
	PaStream *stream;

	PaStream **x = malloc(sizeof(PaStream*));

	PaError err = Pa_Initialize();
	if(err!=paNoError)
		goto error;

	err = Pa_OpenDefaultStream(&stream,2,0,paFloat32,PA_SAMPLE_RATE,paFramesPerBufferUnspecified,paudio_callback,x);
	*x = stream;
	if(err!=paNoError)
		goto error;

	err = Pa_StartStream(stream);
	if(err!=paNoError)
		goto error;

	return 0;

error:
	error("Error opening audio input: %s\n", Pa_GetErrorText(err));
	return 1;
}

int fl_cmp(const void *a, const void *b)
{
	float x = *(float*)a;
	float y = *(float*)b;
	return x<y ? -1 : x>y ? 1 : 0;
}

struct filter {
	double a0,a1,a2,b1,b2;
};

void make_hp(struct filter *f, double freq)
{
	double K = tan(M_PI * freq);
	double norm = 1 / (1 + K * sqrt(2) + K * K);
	f->a0 = 1 * norm;
	f->a1 = -2 * f->a0;
	f->a2 = f->a0;
	f->b1 = 2 * (K * K - 1) * norm;
	f->b2 = (1 - K * sqrt(2) + K * K) * norm;
}

void make_lp(struct filter *f, double freq)
{
	double K = tan(M_PI * freq);
	double norm = 1 / (1 + K * sqrt(2) + K * K);
	f->a0 = K * K * norm;
	f->a1 = 2 * f->a0;
	f->a2 = f->a0;
	f->b1 = 2 * (K * K - 1) * norm;
	f->b2 = (1 - K * sqrt(2) + K * K) * norm;
}

void run_filter(struct filter *f, float *buff, int size)
{
	int i;
	double z1 = 0, z2 = 0;
	for(i=0; i<size; i++) {
		double in = buff[i];
		double out = in * f->a0 + z1;
		z1 = in * f->a1 + z2 - f->b1 * out;
		z2 = in * f->a2 - f->b2 * out;
		buff[i] = out;
	}
}

struct processing_buffers {
	int sample_rate;
	int sample_count;
	float *samples, *samples_sc, *waveform, *waveform_sc;
	fftwf_complex *fft;
	fftwf_plan plan_a, plan_b, plan_c, plan_d;
	struct filter *hpf, *lpf;
	double period,sigma,be,waveform_max;
	int tic,toc;
	int ready;
	uint64_t timestamp, last_tic, last_toc;
};

void setup_buffers(struct processing_buffers *b)
{
	b->samples = fftwf_malloc(2 * b->sample_count * sizeof(float));
	b->samples_sc = malloc(2 * b->sample_count * sizeof(float));
	b->waveform = malloc(2 * b->sample_rate * sizeof(float));
	b->waveform_sc = malloc(2 * b->sample_rate * sizeof(float));
	b->fft = malloc((b->sample_count + 1) * sizeof(fftwf_complex));
	b->plan_a = fftwf_plan_dft_r2c_1d(2 * b->sample_count, b->samples, b->fft, FFTW_ESTIMATE);
	b->plan_b = fftwf_plan_dft_c2r_1d(2 * b->sample_count, b->fft, b->samples_sc, FFTW_ESTIMATE);
	b->plan_c = fftwf_plan_dft_r2c_1d(2 * b->sample_rate, b->waveform, b->fft, FFTW_ESTIMATE);
	b->plan_d = fftwf_plan_dft_c2r_1d(2 * b->sample_rate, b->fft, b->waveform_sc, FFTW_ESTIMATE);
	b->hpf = malloc(sizeof(struct filter));
	make_hp(b->hpf,(double)FILTER_CUTOFF/b->sample_rate);
	b->lpf = malloc(sizeof(struct filter));
	make_lp(b->lpf,(double)FILTER_CUTOFF/b->sample_rate);
	b->ready = 0;
}

struct processing_buffers *pb_clone(struct processing_buffers *p)
{
	struct processing_buffers *new = malloc(sizeof(struct processing_buffers));
	new->sample_rate = p->sample_rate;
	new->waveform = malloc(new->sample_rate * sizeof(float));
	memcpy(new->waveform, p->waveform, new->sample_rate * sizeof(float));

	new->sample_count = p->sample_count;
	new->samples_sc = malloc(new->sample_count * sizeof(float));
	memcpy(new->samples_sc, p->samples_sc, new->sample_rate * sizeof(float));

	new->period = p->period;
	new->sigma = p->sigma;
	new->be = p->be;
	new->waveform_max = p->waveform_max;
	new->tic = p->tic;
	new->toc = p->toc;
	new->ready = p->ready;
	return new;
}

void pb_destroy(struct processing_buffers *p)
{
	// Future BUG: we free only waveform
	free(p->waveform);
	free(p->samples_sc);
	free(p);
}

void noise_suppressor(struct processing_buffers *p)
{
	float *a = p->samples_sc;
	float *b = p->samples_sc + p->sample_count;
	int window = p->sample_rate / 50;
	int i;

	for(i = 0; i < p->sample_count; i++)
		a[i] = p->samples[i] * p->samples[i];

	double r_av = 0;
	for(i = 0; i < window; i++)
		r_av += a[i];
	for(i = 0;; i++) {
		b[i] = r_av;
		if(i + window == p->sample_count) break;
		r_av += a[i + window] - a[i];
	}

	int m = p->sample_count - window + 1;
	float max = 0;
	int j = 0;
	for(i = 0; i < m; i++) {
		if(b[i] > max) max = b[i];
		if((i+1) % (p->sample_rate/2) == 0) {
			a[j++] = max;
			max = 0;
		}
	}
	qsort(a, j, sizeof(float), fl_cmp);
	float k = a[j/2];

	for(i = 0; i < p->sample_count; i++) {
		int j = i - window / 2;
		j = j < 0 ? 0 : j > p->sample_count - window ? p->sample_count - window : j;
		if(b[j] > 2*k) p->samples[i] = 0;
	}
}

void prepare_data(struct processing_buffers *b)
{
	int i;
	int first_fft_size = b->sample_count/2 + 1;

	run_filter(b->hpf, b->samples, b->sample_count);

	noise_suppressor(b);

	for(i=0; i < b->sample_count; i++)
		b->samples[i] = fabs(b->samples[i]);

	run_filter(b->lpf, b->samples, b->sample_count);

	double average = 0;
	for(i=0; i < b->sample_count; i++)
		average += b->samples[i];
	average /= b->sample_count;
	for(i=0; i < b->sample_count; i++)
		b->samples[i] -= average;

	for(i=0; i < b->sample_rate/10; i++) {
		double k = ( 1 - cos(i*M_PI/(b->sample_rate/10)) ) / 2;
		b->samples[i] *= k;
		b->samples[b->sample_count - i - 1] *= k;
	}

	fftwf_execute(b->plan_a);
	for(i=0; i < b->sample_count+1; i++)
			b->fft[i] = b->fft[i] * conj(b->fft[i]);
	fftwf_execute(b->plan_b);
}

int peak_detector(float *buff, struct processing_buffers *p, int a, int b)
{
	int i;
	double max = buff[a];
	int i_max = a;
	for(i=a+1; i<=b; i++) {
		if(buff[i] > max) {
			max = buff[i];
			i_max = i;
		}
	}
	if(max <= 0) return -1;
	int x,y;
	for(x = i_max; x >= a && buff[x] > 0.7*max; x--);
	for(y = i_max; y <= b && buff[y] > 0.7*max; y++);
	if( x < a || y > b || y-x < p->sample_rate / FILTER_CUTOFF) return -1;
	return i_max;
}

double estimate_period(struct processing_buffers *p)
{
	int estimate = peak_detector(p->samples_sc, p, p->sample_rate / 12, p->sample_rate / 2);
	if(estimate == -1) {
		debug("no candidate period\n");
		return -1;
	}
	int a = estimate*3/2 - p->sample_rate / 50;
	int b = estimate*3/2 + p->sample_rate / 50;
	double max = p->samples_sc[a];
	int i;
	for(i=a+1; i<=b; i++)
		if(p->samples_sc[i] > max)
			max = p->samples_sc[i];
	if(max < 0.2 * p->samples_sc[estimate]) {
		if(estimate * 2 < p->sample_rate / 2) {
			debug("double triggered\n");
			return peak_detector(p->samples_sc, p,
					estimate*2 - p->sample_rate / 50,
					estimate*2 + p->sample_rate / 50);
		} else {
			debug("period rejected (immense beat error?)");
			return -1;
		}
	} else return estimate;
}

int compute_period(struct processing_buffers *b, int bph)
{
	double estimate;
	if(bph)
		estimate = peak_detector(b->samples_sc, b,
				7200 * b->sample_rate / bph - b->sample_rate / 100,
				7200 * b->sample_rate / bph + b->sample_rate / 100);
	else
		estimate = estimate_period(b);
	if(estimate == -1) {
		debug("failed to estimate period\n");
		return 1;
	}
	double delta = b->sample_rate * 0.01;
	double new_estimate = estimate;
	double sum = 0;
	double sq_sum = 0;
	int count = 0;
	int cycle = 1;
	for(;;) {
		int inf = floor(new_estimate * cycle - delta);
		int sup = ceil(new_estimate * cycle + delta);
		if(sup > b->sample_count * 2 / 3)
			break;
		new_estimate = peak_detector(b->samples_sc,b,inf,sup);
		if(new_estimate == -1) {
			debug("cycle = %d peak not found\n",cycle);
			return 1;
		}
		new_estimate /= cycle;
		if(new_estimate < estimate - delta || new_estimate > estimate + delta) {
			debug("cycle = %d new_estimate = %f invalid peak\n",cycle,new_estimate/b->sample_rate);
			return 1;
		} else
			debug("cycle = %d new_estimate = %f\n",cycle,new_estimate/b->sample_rate);
		if(inf > b->sample_count / 3) {
			sum += new_estimate;
			sq_sum += new_estimate * new_estimate;
			count++;
		}
		cycle++;
	}
	if(count > 0) estimate = sum / count;
	if(estimate >= b->sample_rate / 2) return 1;
	b->period = estimate;
	if(count > 1)
		b->sigma = sqrt((sq_sum - count * estimate * estimate)/ (count-1));
	else
		b->sigma = b->period;
	return 0;
}

float tmean(float *x, int n)
{
	qsort(x,n,sizeof(float),fl_cmp);
	int i;
	double sum = 0;
	for(i=0; i < n*4/5; i++)
		sum += x[i];
	return sum/(n*4/5);
}

int compute_parameters(struct processing_buffers *p)
{
	int i;
	double x = 0, y = 0;
	for(i=0; i<p->sample_count; i++) {
		double a = i * 4 * M_PI / p->period;
		x += p->samples[i] * cos(a);
		y += p->samples[i] * sin(a);
	}
	double s = p->period * (M_PI + atan2(y,x)) / (4 * M_PI);

	for(i=0; i<2*p->sample_rate; i++)
		p->waveform[i] = 0;

	float bin[(int)ceil(1 + p->sample_count / p->period)];
	for(i=0; i < p->period; i++) {
		int j;
		double k = fmod(i+s,p->period);
		for(j=0;;j++) {
			int n = round(k+j*p->period);
			if(n >= p->sample_count) break;
			bin[j] = p->samples[n];
		}
		p->waveform[i] = tmean(bin,j);
	}

	fftwf_execute(p->plan_c);
	for(i=0; i < p->sample_rate+1; i++)
			p->fft[i] = p->fft[i] * conj(p->fft[i]);
	fftwf_execute(p->plan_d);

	int tic_to_toc = peak_detector(p->waveform_sc,p,
			floor(p->period/2)-p->sample_rate/50,
			floor(p->period/2)+p->sample_rate/50);
	if(tic_to_toc < 0) {
		p->tic = p->toc = -1;
		p->be = -1;
		debug("beat error = ---\n");
		return 1;
	} else {
		p->be = p->period/2 - tic_to_toc;
		debug("beat error = %.1f\n",fabs(p->be)*1000/p->sample_rate);
	}

	double max = 0;
	int max_i = -1;
	for(i=0;i<p->period;i++) {
		if(p->waveform[i] > max) {
			max = p->waveform[i];
			max_i = i;
		}
	}
	p->waveform_max = max;

	if(max_i < p->period/2) {
		p->tic = max_i;
		p->toc = max_i + tic_to_toc;
		if(p->toc > p->period)
			p->toc = floor(p->period);
	} else {
		p->toc = max_i;
		p->tic = max_i - tic_to_toc;
		if(p->tic < 0)
			p->tic = 0;
	}

	double phase = p->timestamp - p->last_tic;
	double apparent_phase = p->sample_count - (s + p->tic);
	double shift = fmod(apparent_phase - phase, p->period);
	if(shift < 0) shift += p->period;
	debug("shift = %.3f\n",shift / p->period);

	if(fabs(shift - p->period/2) < p->period/4) {
		p->last_toc = p->timestamp - (uint64_t)round(fmod(apparent_phase, p->period));
		int t = p->tic;
		p->tic = p->toc;
		p->toc = t;
		apparent_phase = p->sample_count - (s + p->tic);
	} else
		p->last_toc = p->timestamp - (uint64_t)round(fmod(p->sample_count - (s + p->toc), p->period));

	p->last_tic = p->timestamp - (uint64_t)round(fmod(apparent_phase, p->period));

	return 0;
}

void process(struct processing_buffers *p, int bph)
{
	int i;
	prepare_data(p);
	p->ready = ! ( compute_period(p,bph) || compute_parameters(p) );
}

int analyze_pa_data(struct processing_buffers *p, int bph)
{
	static uint64_t last_tic = 0;
	int wp = write_pointer;
	uint64_t ts = timestamp;
	if(wp < 0 || wp >= PA_BUFF_SIZE) wp = 0;
	int i;
	for(i=0; i<NSTEPS; i++) {
		int j,k;
		memset(p[i].samples,0,2 * p[i].sample_count * sizeof(float));
		k = wp - p[i].sample_count;
		if(k < 0) k += PA_BUFF_SIZE;
		for(j=0; j < p[i].sample_count; j++) {
			// p[i].samples[j] = pa_buffers[0][k] + pa_buffers[1][k];
			p[i].samples[j] = pa_buffers[1][k];
			if(++k == PA_BUFF_SIZE) k = 0;
		}
	}
	for(i=0; i<NSTEPS; i++) {
		p[i].timestamp = ts;
		p[i].last_tic = last_tic;
		process(&p[i],bph);
		if( !p[i].ready ) break;
		debug("step %d : %f +- %f\n",i,p[i].period/p[i].sample_rate,p[i].sigma/p[i].sample_rate);
	}
	if(i) {
		last_tic = p[i-1].last_tic;
		debug("%f +- %f\n",p[i-1].period/p[i-1].sample_rate,p[i-1].sigma/p[i-1].sample_rate);
	} else
		debug("---\n");
	return i;
}

struct main_window {
	GtkWidget *window;
	GtkWidget *bph_combo_box;
	GtkWidget *la_spin_button;
	GtkWidget *output_drawing_area;
	GtkWidget *amp_drawing_area;
	GtkWidget *tic_drawing_area;
	GtkWidget *toc_drawing_area;
	GtkWidget *waveform_drawing_area;
	GtkWidget *paperstrip_drawing_area;

	void (*destroy)(GtkWidget *widget, gpointer data);
	struct processing_buffers *(*get_data)(struct main_window *w, int *old);
	void (*recompute)(struct main_window *w);

	int bph;
	int guessed_bph;
	int last_bph;
	double la;

	uint64_t *events;
	int events_wp;

	int signal;

	void *data;
};

cairo_pattern_t *black,*white,*red,*green,*gray,*blue,*yellow,*yellowish,*magenta;

void define_color(cairo_pattern_t **gc,double r,double g,double b)
{
	*gc = cairo_pattern_create_rgb(r,g,b);
}

void initialize_palette()
{
	define_color(&black,0,0,0);
	define_color(&white,1,1,1);
	define_color(&red,1,0,0);
	define_color(&green,0,0.8,0);
	define_color(&gray,0.5,0.5,0.5);
	define_color(&blue,0,0,1);
	define_color(&yellow,1,1,0);
	define_color(&yellowish,0.5,0.5,0);
	define_color(&magenta,1,0,1);
}

gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return FALSE;
}

void draw_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	int width = da->allocation.width;
	int height = da->allocation.height;

	int n;

	int first = 1;
	for(n=0; n<2*width; n++) {
		int i = n < width ? n : 2*width - 1 - n;
		int j = round(a + i * (b-a) / width);
		double y;

		if(j < 0 || j >= p->period || p->waveform[j] <= 0) y = 0;
		else y = p->waveform[j] * 0.4 / p->waveform_max;

		int k = round(y*height);
		if(n < width) k = -k;

		if(first) {
			cairo_move_to(c,i+.5,height/2+k+.5);
			first = 0;
		} else
			cairo_line_to(c,i+.5,height/2+k+.5);
	}
}

void draw_amp_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	int width = da->allocation.width;
	int height = da->allocation.height;

	int i;
	float max = 0;

	int ai = round(a);
	int bi = 1+round(b);
	if(ai < 0) ai = 0;
	if(bi > p->sample_count) bi = p->sample_count;
	for(i=ai; i<bi; i++)
		if(p->samples_sc[i] > max) max = p->samples_sc[i];

	int first = 1;
	for(i=0; i<width; i++) {
		if( round(a + i*(b-a)/width) != round(a + (i+1)*(b-a)/width) ) {
			int j = round(a + i*(b-a)/width);
			if(j < 0) j = 0;
			if(j >= p->sample_count) j = p->sample_count-1;

			int k = round((0.1+p->samples_sc[j]/max)*0.8*height);

			if(first) {
				cairo_move_to(c,i+.5,height-k-.5);
				first = 0;
			} else
				cairo_line_to(c,i+.5,height-k-.5);
		}
	}
}

double amplitude_to_time(double lift_angle, double amp)
{
	return asin(lift_angle / (2 * amp)) / M_PI;
}

int guess_bph(double period)
{
	double bph = 7200 / period;
	double min = bph;
	int i,ret;

	ret = 0;
	for(i=0; preset_bph[i]; i++) {
		double diff = fabs(bph - preset_bph[i]);
		if(diff < min) {
			min = diff;
			ret = i;
		}
	}

	return preset_bph[ret];
}

void draw_watch_icon(cairo_t *c, int happy)
{
	happy = !!happy;
	cairo_set_line_width(c,3);
	cairo_set_source(c,happy?green:red);
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.75, OUTPUT_WINDOW_HEIGHT * (0.75 - 0.5*happy));
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.35, OUTPUT_WINDOW_HEIGHT * (0.65 - 0.3*happy));
	cairo_stroke(c);
	cairo_arc(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.4, 0, 2*M_PI);
	cairo_stroke(c);
}

gboolean output_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	c = gdk_cairo_create(widget->window);
	cairo_set_font_size(c,OUTPUT_FONT);

	cairo_set_source(c,black);
	cairo_paint(c);

	draw_watch_icon(c,w->signal);

	int old;
	struct processing_buffers *p = w->get_data(w,&old);

	char rates[100];
	char bphs[100];

	if(p) {
		int bph = w->guessed_bph;
		double rate = (7200/(bph*p->period / p->sample_rate) - 1)*24*3600;
		double be = fabs(p->be) * 1000 / p->sample_rate;
		rate = round(rate);
		if(rate == 0) rate = 0;
		sprintf(rates,"%s%.0f s/d   %.1f ms   ",rate > 0 ? "+" : "",rate,be);
		sprintf(bphs,"%d bph",bph);
	} else {
		strcpy(rates,"--- s/d   --- ms    ");
		sprintf(bphs,"%d bph",w->guessed_bph);
	}

	if(p && old)
		cairo_set_source(c,yellow);
	else
		cairo_set_source(c,white);

	cairo_text_extents_t extents;

	cairo_text_extents(c,"0",&extents);
	double x = OUTPUT_WINDOW_HEIGHT + (double)OUTPUT_FONT/2;
	double y = (double)OUTPUT_WINDOW_HEIGHT/2 - extents.y_bearing - extents.height/2;

	cairo_move_to(c,x,y);
	cairo_show_text(c,rates);
	cairo_text_extents(c,rates,&extents);
	x += extents.x_advance;

	cairo_set_source(c,white);
	cairo_move_to(c,x,y);
	cairo_show_text(c,bphs);
	cairo_text_extents(c,bphs,&extents);
	x += extents.x_advance;

	cairo_destroy(c);

	return FALSE;
}

struct interactive_w_data {
	struct processing_buffers *bfs;
	struct processing_buffers *old;
};

gboolean amp_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	int width = w->amp_drawing_area->allocation.width;
	int height = w->amp_drawing_area->allocation.height;
	int font = width / 90;
	if(font < 12)
		font = 12;
	int i;
	double span = amplitude_to_time(w->la,120);

	c = gdk_cairo_create(widget->window);
	cairo_set_line_width(c,1);
	cairo_set_font_size(c,font);

	cairo_set_source(c,black);
	cairo_paint(c);

	int old = 0;
	//struct processing_buffers *p = w->get_data(w,&old);
	struct processing_buffers *p = ((struct interactive_w_data *)w->data)->bfs;

	if(p) {
		double span_time = p->period * span;

		double a = p->period - span_time;
		double b = p->period + span_time;

		draw_amp_graph(a,b,c,p,w->amp_drawing_area);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke(c);
	}

	cairo_destroy(c);

	return FALSE;
}

void expose_waveform(cairo_t *c, struct main_window *w, GtkWidget *da, int (*get_offset)(struct processing_buffers*))
{
	int width = da->allocation.width;
	int height = da->allocation.height;
	int font = width / 90;
	if(font < 12)
		font = 12;
	int i;

	cairo_set_line_width(c,1);
	cairo_set_font_size(c,font);

	cairo_set_source(c,black);
	cairo_paint(c);

	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
		cairo_move_to(c, x + .5, height / 2 + .5);
		cairo_line_to(c, x + .5, height - .5);
		if(i%5)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}
	cairo_set_source(c,white);
	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
		if(!(i%5)) {
			char s[10];
			sprintf(s,"%d",i);
			cairo_move_to(c,x+font/4,height-font/2);
			cairo_show_text(c,s);
		}
	}

	int old;
	struct processing_buffers *p = w->get_data(w,&old);
	double period = p ? p->period / p->sample_rate : 7200. / w->guessed_bph;

	for(i = 10; i < 360; i+=10) {
		if(2*i < w->la) continue;
		double t = period*amplitude_to_time(w->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height / 2 + .5);
		if(i % 50)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}
	cairo_set_source(c,white);
	for(i = 50; i < 360; i+=50) {
		double t = period*amplitude_to_time(w->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		char s[10];
		sprintf(s,"%d",abs(i));
		cairo_move_to(c,x+font/4,font * 3 / 2);
		cairo_show_text(c,s);
	}

	if(p) {
		double span = 0.001 * p->sample_rate;
		int offset = get_offset(p);

		double a = offset - span * NEGATIVE_SPAN;
		double b = offset + span * POSITIVE_SPAN;

		draw_graph(a,b,c,p,da);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		cairo_set_source(c,yellow);
		cairo_stroke(c);
	}

//	cairo_set_source(c,white);
//	cairo_move_to(c,font/2,3*font/2);
//	cairo_show_text(c,"Beat error (ms)");

	cairo_destroy(c);
}

int get_tic(struct processing_buffers *p)
{
	return p->tic;
}

int get_toc(struct processing_buffers *p)
{
	return p->toc;
}

gboolean tic_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	expose_waveform(gdk_cairo_create(widget->window), w, w->tic_drawing_area, get_tic);
	return FALSE;
}

gboolean toc_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	expose_waveform(gdk_cairo_create(widget->window), w, w->toc_drawing_area, get_toc);
	return FALSE;
}

gboolean waveform_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	int width = w->waveform_drawing_area->allocation.width;
	int height = w->waveform_drawing_area->allocation.height;
	int font = width / 90;
	if(font < 12)
		font = 12;

	c = gdk_cairo_create(widget->window);
	cairo_set_line_width(c,1);
	cairo_set_font_size(c,font);

	cairo_set_source(c,black);
	cairo_paint(c);

	int old;
	struct processing_buffers *p = w->get_data(w,&old);

	if(p) {
		int i;
		float max = 0;
		int max_i = 0;

		for(i=0; i<p->period; i++)
			if(p->waveform[i] > max) {
				max = p->waveform[i];
				max_i = i;
			}

		int first = 1;
		for(i=0; i<width; i++) {
			if( round(i*p->period/width) != round((i+1)*p->period/width) ) {
				int j = round(i*p->period/width);
				//j = fmod(j + max_i, p->period);
				if(j < 0) j = 0;
				if(j >= p->sample_count) j = p->sample_count-1;

				int k = round((p->waveform[j]+max/10)*(height-1)/(max*1.1));
				if(k < 0) k = 0;
				if(k >= height) k = height-1;

				if(first) {
					cairo_move_to(c,i+.5,height-k-.5);
					first = 0;
				} else
					cairo_line_to(c,i+.5,height-k-.5);
			}
		}
		cairo_set_source(c,old?yellow:white);
		cairo_stroke(c);
	}

	cairo_destroy(c);

	return FALSE;
}

gboolean paperstrip_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	int i,old;
	struct processing_buffers *p = w->get_data(w,&old);

	if(p && !old) {
		uint64_t first, second;
		if(p->last_tic < p->last_toc) {
			first = p->last_tic;
			second = p->last_toc;
		} else {
			first = p->last_toc;
			second = p->last_tic;
		}
		uint64_t threshold = w->events[w->events_wp] + (int)round(p->period / 4);
		for(i=5; i>=0; i--) {
			uint64_t delta = round(p->period * i);
			if(first - delta > threshold) {
				if(++w->events_wp == EVENTS_COUNT) w->events_wp = 0;
				w->events[w->events_wp] = first - delta;
				debug("event at %llu\n",w->events[w->events_wp]);
			}
			if(second - delta > threshold) {
				if(++w->events_wp == EVENTS_COUNT) w->events_wp = 0;
				w->events[w->events_wp] = second - delta;
				debug("event at %llu\n",w->events[w->events_wp]);
			}
		}
	}

	cairo_t *c;

	int width = w->paperstrip_drawing_area->allocation.width;
	int height = w->paperstrip_drawing_area->allocation.height;

	c = gdk_cairo_create(widget->window);

	cairo_set_source(c,black);
	cairo_paint(c);

	cairo_set_source(c,white);

	int strip_width = width * 9 / 10;
	double sweep = PA_SAMPLE_RATE * 3600. / w->guessed_bph;
	double now = sweep*ceil(timestamp/sweep);
	for(i = w->events_wp;;) {
		if(!w->events[i]) break;
		int column = floor(fmod(now - w->events[i], sweep) * strip_width / sweep);
		int row = floor((now - w->events[i]) / sweep);
		if(row >= height) break;
		cairo_move_to(c,column,row);
		cairo_line_to(c,column+1,row);
		cairo_line_to(c,column+1,row+1);
		cairo_line_to(c,column,row+1);
		cairo_line_to(c,column,row);
		cairo_fill(c);
		if(column < width - strip_width && row > 0) {
			column += strip_width;
			row -= 1;
			cairo_move_to(c,column,row);
			cairo_line_to(c,column+1,row);
			cairo_line_to(c,column+1,row+1);
			cairo_line_to(c,column,row+1);
			cairo_line_to(c,column,row);
			cairo_fill(c);
		}
		if(--i < 0) i = EVENTS_COUNT - 1;
		if(i == w->events_wp) break;
	}

	cairo_destroy(c);

	return FALSE;
}

void redraw(struct main_window *w)
{
	gtk_widget_queue_draw_area(w->output_drawing_area,0,0,w->output_drawing_area->allocation.width,w->output_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->amp_drawing_area,0,0,w->amp_drawing_area->allocation.width,w->amp_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->tic_drawing_area,0,0,w->tic_drawing_area->allocation.width,w->tic_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->toc_drawing_area,0,0,w->toc_drawing_area->allocation.width,w->toc_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->waveform_drawing_area,0,0,w->waveform_drawing_area->allocation.width,w->waveform_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->paperstrip_drawing_area,0,0,w->paperstrip_drawing_area->allocation.width,w->paperstrip_drawing_area->allocation.height);
}

void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
	char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));
	if(s) {
		int n;
		char *t;
		n = strtol(s,&t,10);
		if(*t || n < MIN_BPH || n > MAX_BPH) w->bph = 0;
		else w->bph = w->guessed_bph = n;
		g_free(s);
		w->recompute(w);
		redraw(w);
	}
}

void handle_la_change(GtkSpinButton *b, struct main_window *w)
{
	double la = gtk_spin_button_get_value(b);
	if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
	w->la = la;
	redraw(w);
}

void init_main_window(struct main_window *w)
{
	w->signal = 0;

	w->events = malloc(EVENTS_COUNT * sizeof(uint64_t));
	memset(w->events,0,EVENTS_COUNT * sizeof(uint64_t));
	w->events_wp = 0;

	w->guessed_bph = w->last_bph = DEFAULT_BPH;
	w->bph = 0;
	w->la = DEFAULT_LA;

	w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(w->window),10);
	g_signal_connect(G_OBJECT(w->window),"delete_event",G_CALLBACK(delete_event),NULL);
	g_signal_connect(G_OBJECT(w->window),"destroy",G_CALLBACK(w->destroy),w);

	GtkWidget *vbox = gtk_vbox_new(FALSE,10);
	gtk_container_add(GTK_CONTAINER(w->window),vbox);
	gtk_widget_show(vbox);

	GtkWidget *hbox = gtk_hbox_new(FALSE,10);
	gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,TRUE,0);
	gtk_widget_show(hbox);

	GtkWidget *label = gtk_label_new("bph");
	GTK_WIDGET_SET_FLAGS(label,GTK_NO_WINDOW);
	gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
	gtk_widget_show(label);

	w->bph_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_box_pack_start(GTK_BOX(hbox),w->bph_combo_box,FALSE,TRUE,0);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box),"guess");
	int *bph;
	for(bph = preset_bph; *bph; bph++) {
		char s[100];
		sprintf(s,"%d",*bph);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box),s);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box),0);
	gtk_signal_connect(GTK_OBJECT(w->bph_combo_box),"changed",(GtkSignalFunc)handle_bph_change,w);
	gtk_widget_show(w->bph_combo_box);

	label = gtk_label_new("lift angle");
	GTK_WIDGET_SET_FLAGS(label,GTK_NO_WINDOW);
	gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
	gtk_widget_show(label);

	w->la_spin_button = gtk_spin_button_new_with_range(MIN_LA,MAX_LA,1);
	gtk_box_pack_start(GTK_BOX(hbox),w->la_spin_button,FALSE,TRUE,0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button),DEFAULT_LA);
	gtk_signal_connect(GTK_OBJECT(w->la_spin_button),"value_changed",(GtkSignalFunc)handle_la_change,w);
	gtk_widget_show(w->la_spin_button);

	GtkWidget *hbox2 = gtk_hbox_new(FALSE,10);
	gtk_box_pack_start(GTK_BOX(vbox),hbox2,TRUE,TRUE,0);
	gtk_widget_show(hbox2);

	w->paperstrip_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->paperstrip_drawing_area),200,500);
	gtk_box_pack_start(GTK_BOX(hbox2),w->paperstrip_drawing_area,FALSE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->paperstrip_drawing_area),"expose_event",
			(GtkSignalFunc)paperstrip_expose_event, w);
	gtk_widget_set_events(w->paperstrip_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->paperstrip_drawing_area);

	GtkWidget *vbox2 = gtk_vbox_new(FALSE,10);
	gtk_box_pack_start(GTK_BOX(hbox2),vbox2,TRUE,TRUE,0);
	gtk_widget_show(vbox2);

	w->output_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->output_drawing_area),500,OUTPUT_WINDOW_HEIGHT);
	gtk_box_pack_start(GTK_BOX(vbox2),w->output_drawing_area,FALSE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->output_drawing_area),"expose_event",
			(GtkSignalFunc)output_expose_event, w);
	gtk_widget_set_events(w->output_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->output_drawing_area);

	w->tic_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->tic_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox2),w->tic_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->tic_drawing_area),"expose_event",
			(GtkSignalFunc)tic_expose_event, w);
	gtk_widget_set_events(w->tic_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->tic_drawing_area);

	w->toc_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->toc_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox2),w->toc_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->toc_drawing_area),"expose_event",
			(GtkSignalFunc)toc_expose_event, w);
	gtk_widget_set_events(w->toc_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->toc_drawing_area);

	w->amp_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->amp_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox2),w->amp_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->amp_drawing_area),"expose_event",
			(GtkSignalFunc)amp_expose_event, w);
	gtk_widget_set_events(w->amp_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->amp_drawing_area);

	w->waveform_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->waveform_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox2),w->waveform_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->waveform_drawing_area),"expose_event",
			(GtkSignalFunc)waveform_expose_event, w);
	gtk_widget_set_events(w->waveform_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->waveform_drawing_area);

	gtk_window_maximize(GTK_WINDOW(w->window));
	gtk_widget_show(w->window);
}

void quit()
{
	gtk_main_quit();
}

struct processing_buffers *int_get_data(struct main_window *w, int *old)
{
	struct interactive_w_data *d = w->data;
	struct processing_buffers *p = d->bfs;
	int i;
	for(i=0; i<NSTEPS && p[i].ready; i++);
	if(i && p[i-1].sigma < p[i-1].period / 10000) {
		if(d->old) pb_destroy(d->old);
		d->old = pb_clone(&p[i-1]);
		*old = 0;
		return &p[i-1];
	} else {
		*old = 1;
		return d->old;
	}
}

void int_recompute(struct main_window *w)
{
	struct processing_buffers *p = ((struct interactive_w_data *)w->data)->bfs;
	w->signal = analyze_pa_data(p, w->bph);
	int old;
	p = int_get_data(w,&old);
	if(p)
		w->guessed_bph = w->bph ? w->bph : guess_bph(p->period / p->sample_rate);
}

guint refresh(struct main_window *w)
{
	w->recompute(w);
	redraw(w);
	return TRUE;
}

int run_interactively()
{
	struct processing_buffers p[NSTEPS];
	int i;
	for(i=0; i<NSTEPS; i++) {
		p[i].sample_rate = PA_SAMPLE_RATE;
		p[i].sample_count = PA_SAMPLE_RATE * (1<<(i+FIRST_STEP));
		setup_buffers(&p[i]);
		p[i].period = -1;
	}
	if(start_portaudio()) return 1;

	struct main_window w;
	struct interactive_w_data d;
	d.bfs = p;
	d.old = NULL;
	w.destroy = quit;
	w.data = &d;
	w.get_data = int_get_data;
	w.recompute = int_recompute;

	init_main_window(&w);

	g_timeout_add(round(1000*(sqrt(5)-1)/2),(GSourceFunc)refresh,&w);

	gtk_main();

	return 0;
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);
	initialize_palette();

	return run_interactively();
}
