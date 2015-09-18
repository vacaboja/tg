#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sndfile.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <stdint.h>
#include <portaudio.h>
#include <stdarg.h>
#include <gtk/gtk.h>

#define FILTER_CUTOFF 3000
//#define HIGHPASS 10

#define NSTEPS 5
#define FIRST_STEP 0
#define MIN_STEP 1
#define PA_SAMPLE_RATE 44100
#define PA_BUFF_SIZE (PA_SAMPLE_RATE << (NSTEPS + FIRST_STEP))

#define FPS 2
#define AMPLITUDE_WIDTH .07

#define OUTPUT_FONT 50
#define OUTPUT_WINDOW_HEIGHT 80

#define MIN_BPH 12000
#define MAX_BPH 36000
#define MIN_LA 20
#define MAX_LA 90
#define DEFAULT_LA 52

int preset_bph[] = { 12000, 14400, 18000, 19800, 21600, 25200, 28800, 36000, 0 };

volatile float pa_buffers[2][PA_BUFF_SIZE];
volatile int write_pointer = 0;

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
		if(write_pointer < PA_BUFF_SIZE - 1) write_pointer++;
		else write_pointer = 0;
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
	float *samples;
	float *filtered_samples;
	float *waveform;
	fftwf_complex *fft;
	fftwf_plan plan_a, plan_b, plan_c, plan_d;
	struct filter *hpf, *lpf;
	double period,sigma,be;
	int tic,toc;
	int accept;
};

void setup_buffers(struct processing_buffers *b)
{
	b->samples = fftwf_malloc(2 * b->sample_count * sizeof(float));
	b->filtered_samples = malloc(b->sample_count * sizeof(float));
	b->waveform = malloc(b->sample_count * sizeof(float));
	b->fft = malloc((b->sample_count + 1) * sizeof(fftwf_complex));
	b->plan_a = fftwf_plan_dft_r2c_1d(b->sample_count, b->samples, b->fft, FFTW_ESTIMATE);
	b->plan_b = fftwf_plan_dft_c2r_1d(b->sample_count, b->fft, b->samples, FFTW_ESTIMATE);
	b->plan_c = fftwf_plan_dft_r2c_1d(2 * b->sample_count, b->samples, b->fft, FFTW_ESTIMATE);
	b->plan_d = fftwf_plan_dft_c2r_1d(2 * b->sample_count, b->fft, b->samples, FFTW_ESTIMATE);
	b->hpf = malloc(sizeof(struct filter));
	make_hp(b->hpf,(double)FILTER_CUTOFF/b->sample_rate);
	b->lpf = malloc(sizeof(struct filter));
	make_lp(b->lpf,(double)FILTER_CUTOFF/b->sample_rate);
}

struct processing_buffers *pb_clone(struct processing_buffers *p)
{
	struct processing_buffers *new = malloc(sizeof(struct processing_buffers));
	new->sample_rate = p->sample_rate;
	new->sample_count = p->sample_count;
	new->samples = malloc(new->sample_count * sizeof(float));
	memcpy(new->samples, p->samples, new->sample_count * sizeof(float));
	new->filtered_samples = malloc(new->sample_count * sizeof(float));
	memcpy(new->filtered_samples, p->filtered_samples, new->sample_count * sizeof(float));
	new->waveform = malloc(new->sample_count * sizeof(float));
	memcpy(new->waveform, p->waveform, new->sample_count * sizeof(float));
	new->fft = NULL;
	new->plan_a = new->plan_b = new->plan_c = new->plan_d = NULL;
	new->period = p->period;
	new->sigma = p->sigma;
	new->be = p->be;
	new->tic = p->tic;
	new->toc = p->toc;
	new->accept = p->accept;
	return new;
}

void pb_destroy(struct processing_buffers *p)
{
	// Future BUG: we free only samples, f_samp, and waveform
	free(p->samples);
	free(p->filtered_samples);
	free(p->waveform);
	free(p);
}

/*
int float_cmp(const void *a, const void *b)
{
	float x = *(float*)a;
	float y = *(float*)b;
	if(x < y) return -1;
	if(x > y) return 1;
	return 0;
}
*/

void compute_self_correlation(struct processing_buffers *b)
{
	int i;
	int first_fft_size = b->sample_count/2 + 1;
/*	
	fftwf_execute(b->plan_a);
	for(i=0; i < b->sample_count/2 + 1; i++) {
		if((uint64_t)b->sample_rate * i < (uint64_t)FILTER_CUTOFF * b->sample_count)
			b->fft[i] = 0;
	}
	fftwf_execute(b->plan_b);
*/	
	run_filter(b->hpf, b->samples, b->sample_count);

	for(i=0; i < b->sample_count; i++)
		b->samples[i] = fabs(b->samples[i]);
/*
	float min = 1e20;
	for(i=0; i + b->sample_rate <= b->sample_count; i += b->sample_rate) {
		int j;
		float max = 0;
		for(j=0; j < b->sample_rate; j++)
			if(b->samples[i+j] > max) max = b->samples[i+j];
		if(max < min) min = max;
	}

	int max_count = 0;
	int ms = b->sample_rate/1000;
	for(i=0; i<10*ms; i++) {
		if(b->samples[i] >= min) {
			if(max_count < 110*ms)
				max_count += 100;
		} else if(max_count > 0)
			max_count--;
	}
	for(i=0; i < b->sample_count; i++) {
		if(b->samples[i] > min) b->samples[i] = min;
		if(max_count >= 100*ms) b->samples[i] = 0;
		if(b->samples[i+10*ms] >= min) {
			if(max_count < 110*ms)
				max_count += 100;
		} else if(max_count > 0)
			max_count--;
	}
*/
/*
	for(i=0; i < b->sample_count; i++)
		b->filtered_samples[i] = b->samples[i];
	qsort(b->filtered_samples,b->sample_count,sizeof(float),float_cmp);
	float median = b->filtered_samples[b->sample_count / 2];
	for(i=0; i < b->sample_count; i++)
		b->samples[i] -= median;
*/

	run_filter(b->lpf, b->samples, b->sample_count);

	double average = 0;
	for(i=0; i < b->sample_count; i++)
		average += b->samples[i];
	average /= b->sample_count;
	for(i=0; i < b->sample_count; i++)
		b->samples[i] -= average;

	for(i=0; i < b->sample_count; i++)
		b->filtered_samples[i] = b->samples[i];

	fftwf_execute(b->plan_c);
	for(i=0; i < b->sample_count+1; i++) {
//		if(  (uint64_t)b->sample_rate * i < (uint64_t)FILTER_CUTOFF * b->sample_count )
//				&&
//		     (uint64_t)b->sample_rate * i > (uint64_t)HIGHPASS * b->sample_count  )
			b->fft[i] = b->fft[i] * conj(b->fft[i]);
//		else
//			b->fft[i] = 0;
	}
	fftwf_execute(b->plan_d);
}

int peak_detector(struct processing_buffers *p, int a, int b)
{
	int i;
	double max = p->samples[a];
	int i_max = a;
	for(i=a+1; i<=b; i++) {
		if(p->samples[i] > max) {
			max = p->samples[i];
			i_max = i;
		}
	}
	if(max <= 0) return -1;
	int x,y;
	for(x = i_max; x >= a && p->samples[x] > 0.7*max; x--);
	for(y = i_max; y <= b && p->samples[y] > 0.7*max; y++);
	if( x < a || y > b || y-x < p->sample_rate / FILTER_CUTOFF) return -1;
	return i_max;
}

double estimate_period(struct processing_buffers *p)
{
	int estimate = peak_detector(p, p->sample_rate / 12, p->sample_rate / 2);
	if(estimate == -1) return -1;
	int a = estimate*3/2 - p->sample_rate / 100;
	int b = estimate*3/2 + p->sample_rate / 100;
	double max = p->samples[a];
	int i;
	for(i=a+1; i<=b; i++)
		if(p->samples[i] > max)
			max = p->samples[i];
	if(max < 0.4 * p->samples[estimate]) {
		if(estimate * 2 < p->sample_rate / 2) {
			debug("double triggered\n");
			return peak_detector(p, estimate*2 - p->sample_rate / 100, estimate*2 + p->sample_rate / 100);
		} else
			return -1;
	} else return estimate;
}

int compute_period(struct processing_buffers *b, int bph)
{
	double estimate;
	if(bph)
		estimate = peak_detector(b, 7200 * b->sample_rate / bph - b->sample_rate / 100, 7200 * b->sample_rate / bph + b->sample_rate / 100);
	else
		estimate = estimate_period(b);
	if(estimate == -1) return -1;
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
		new_estimate = peak_detector(b,inf,sup);
		if(new_estimate == -1) {
			debug("cycle = %d peak not found\n",cycle);
			return -1;
		}
		new_estimate /= cycle;
		if(new_estimate < estimate - delta || new_estimate > estimate + delta) {
			debug("cycle = %d new_estimate = %f invalid peak\n",cycle,new_estimate/b->sample_rate);
			return -1;
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
	b->period = estimate;
	if(count > 1)
		b->sigma = sqrt((sq_sum - count * estimate * estimate)/ (count-1));
	else
		b->sigma = b->period;
	return 0;
}

void compute_parameters(struct processing_buffers *p)
{
	int i;
	double x = 0, y = 0;
	for(i=0; i<p->sample_count; i++) {
		double a = i * 4 * M_PI / p->period;
		x += p->filtered_samples[i] * cos(a);
		y += p->filtered_samples[i] * sin(a);
	}
	double s = p->period * (M_PI + atan2(y,x)) / (4 * M_PI);

	for(i=0; i<2*p->sample_count; i++)
		p->samples[i] = 0;
	for(i=0; i < p->sample_count; i++) {
		int j = floor(fmod(i+p->period-s,p->period));
		p->samples[j] += p->filtered_samples[i];
	}

	fftwf_execute(p->plan_c);
	for(i=0; i < p->sample_count+1; i++) {
//		if(  (uint64_t)p->sample_rate * i < (uint64_t)FILTER_CUTOFF * p->sample_count )
//				&&
//		     (uint64_t)p->sample_rate * i > (uint64_t)HIGHPASS * p->sample_count  )
			p->fft[i] = p->fft[i] * conj(p->fft[i]);
//		else
//			p->fft[i] = 0;
	}
	fftwf_execute(p->plan_d);

	int tic_to_toc = peak_detector(p,floor(p->period/2)-p->sample_rate/100,floor(p->period/2)+p->sample_rate/100);
	if(tic_to_toc < 0) {
		p->tic = p->toc = -1;
		p->be = -1;
		debug("beat error = ---\n");
		return;
	} else {
		p->be = p->period/2 - tic_to_toc;
		debug("beat error = %.1f\n",fabs(p->be)*1000/p->sample_rate);
	}

	for(i=0; i<2*p->sample_count; i++)
		p->samples[i] = 0;
	for(i=0; i < p->sample_count; i++) {
		int j = floor(fmod(i+p->period-s,p->period));
		p->samples[j] += p->filtered_samples[i];
	}
/*
	fftwf_execute(p->plan_c);
	for(i=0; i < p->sample_count+1; i++) {
		if(  (uint64_t)p->sample_rate * i > (uint64_t)FILTER_CUTOFF * p->sample_count ) 
			p->fft[i] = 0;
	}
	fftwf_execute(p->plan_d);
*/
	double max = 0;
	int max_i = -1;
	for(i=0;i<p->period;i++)
		if(p->samples[i] > max) {
			max = p->samples[i];
			max_i = i;
		}

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
}

/*
void save_debug(struct processing_buffers *p)
{
	SF_INFO out_info;
	SNDFILE *out_sfile;

	out_info.samplerate = p->sample_rate;
	out_info.channels = 1;
	out_info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

	out_sfile = sf_open("out.wav",SFM_WRITE,&out_info);
	if(!out_sfile) {
		error("Can not open out.wav\n");
		return;
	}

	float max_sample = 0;
	int i;
	for(i = 100; i < p->sample_count; i++) {
		float x = p->samples[i] >= 0 ? p->samples[i] : - p->samples[i];
		max_sample = max_sample > x ? max_sample : x;
	}
	for(i = 0; i < p->sample_count; i++)
		p->samples[i] /= max_sample;

	debug("Written %d samples to out.wav\n",sf_write_float(out_sfile, p->samples, p->sample_count));

	sf_close(out_sfile);
}

int process_file(char *filename, struct processing_buffers *p)
{
	SF_INFO sfinfo;
	SNDFILE *sfile;

	sfinfo.format = 0;

	sfile = sf_open(filename,SFM_READ,&sfinfo);
	if(!sfile) {
		error("Error opening file %s : %s\n",filename,sf_strerror(NULL));
		return 1;
	}

	if(sfinfo.channels != 1) {
		error("Channel count = %d != 1\n",sfinfo.channels);
		return 1;
	}

	p->sample_rate = sfinfo.samplerate;
	p->sample_count = sfinfo.frames;

	setup_buffers(p);

	memset(p->samples,0,2 * p->sample_count * sizeof(float));

	if(sfinfo.frames != sf_read_float(sfile,p->samples,p->sample_count)) {
		error("Error reading file %s\n",filename);
		return 1;
	}

	sf_close(sfile);

	compute_self_correlation(p);

//	save_debug(&b);

	return 0;
}
*/
int acceptable(struct processing_buffers *p)
{
	return p->sigma < p->period / 10000;
}

void analyze_pa_data(struct processing_buffers *p, int bph)
{
	int wp = write_pointer;
	if(wp < 0 || wp >= PA_BUFF_SIZE) wp = 0;
	int i;
	for(i=0; i<NSTEPS; i++) {
		int j,k;
		memset(p[i].samples,0,2 * p[i].sample_count * sizeof(float));
		k = wp - p[i].sample_count;
		if(k < 0) k += PA_BUFF_SIZE;
		for(j=0; j < p[i].sample_count; j++) {
			p[i].samples[j] = pa_buffers[0][k] + pa_buffers[1][k];
			if(++k == PA_BUFF_SIZE) k = 0;
		}
	}
	for(i=0; i<NSTEPS; i++) {
		compute_self_correlation(&p[i]);

		p[i].period = -1;
		if( compute_period(&p[i],bph) ) break;

		compute_parameters(&p[i]);
			
		debug("step %d : %f +- %f\n",i,p[i].period/p[i].sample_rate,p[i].sigma/p[i].sample_rate);
//		save_debug(&p[i]);
	}
	if(i) {
		p[i-1].accept = i > MIN_STEP && acceptable(&p[i-1]);
		if(p[i-1].accept)
			debug("%f +- %f\n",p[i-1].period/p[i-1].sample_rate,p[i-1].sigma/p[i-1].sample_rate);
		else
			debug("---\n");
	} else
		debug("---\n");
}

struct main_window {
	GtkWidget *window;
	GtkWidget *bph_combo_box;
	GtkWidget *la_spin_button;
	GtkWidget *output_drawing_area;
	GtkWidget *amp_drawing_area;
	GtkWidget *be_drawing_area;
	GtkWidget *waveform_drawing_area;

	void (*destroy)(GtkWidget *widget, gpointer data);
	struct processing_buffers *(*get_data)(struct main_window *w, int *old);
	void (*recompute)(struct main_window *w);

	int bph;
	double la;

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

	int i;
	float max = 0;

	int ai = round(a);
	int bi = 1+round(b);
	if(ai < 0) ai = 0;
	if(bi > p->sample_count) bi = p->sample_count;
	for(i=ai; i<bi; i++)
		if(p->samples[i] > max) max = p->samples[i];

	int first = 1;
	for(i=0; i<width; i++) {
		if( round(a + i*(b-a)/width) != round(a + (i+1)*(b-a)/width) ) {
			int j = round(a + i*(b-a)/width);
			if(j < 0) j = 0;
			if(j >= p->sample_count) j = p->sample_count-1;

			int k = round(fabs(p->samples[j])*0.45*height/max);

			if(first) {
				cairo_move_to(c,i+.5,height/2-k-.5);
				first = 0;
			} else
				cairo_line_to(c,i+.5,height/2-k-.5);
		}
	}
	first = 1;
	for(i=0; i<width; i++) {
		if( round(a + i*(b-a)/width) != round(a + (i+1)*(b-a)/width) ) {
			int j = round(a + i*(b-a)/width);
			if(j < 0) j = 0;
			if(j >= p->sample_count) j = p->sample_count-1;

			int k = round(fabs(p->samples[j])*0.45*height/max);

			if(first) {
				cairo_move_to(c,i+.5,height/2+k-.5);
				first = 0;
			} else
				cairo_line_to(c,i+.5,height/2+k-.5);
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

gboolean output_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	c = gdk_cairo_create(widget->window);
	cairo_set_font_size(c,OUTPUT_FONT);

	cairo_set_source(c,black);
	cairo_paint(c);

	int old;
	struct processing_buffers *p = w->get_data(w,&old);

	char rates[100];
	char bphs[100];

	if(p) {
		int bph = w->bph ? w->bph : guess_bph(p->period / p->sample_rate);
		double rate = (7200/(bph*p->period / p->sample_rate) - 1)*24*3600;
		double be = fabs(p->be) * 1000 / p->sample_rate;
		rate = round(rate);
		if(rate == 0) rate = 0;
		sprintf(rates,"%s%.0f s/d   %.1f ms   ",rate > 0 ? "+" : "",rate,be);
		sprintf(bphs,"%d bph",bph);
	} else {
		strcpy(rates,"---   ");
		if(w->bph)
			sprintf(bphs,"bph = %d",w->bph);
		else
			strcpy(bphs,"bph = ---");
	}

	if(p && old)
		cairo_set_source(c,yellow);
	else
		cairo_set_source(c,white);

	cairo_text_extents_t extents;

	cairo_text_extents(c,"0",&extents);
	double x = (double)OUTPUT_FONT/2;
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

	for(i = 40; i < 360; i+=10) {
		double t = amplitude_to_time(w->la,i);
		if(t > span) continue;
		int x1 = round(width * (.5-.5*t/span));
		int x2 = round(width * (.5+.5*t/span));
		cairo_move_to(c, x1+.5, .5);
		cairo_line_to(c, x1+.5, height-.5);
		cairo_move_to(c, x2+.5, .5);
		cairo_line_to(c, x2+.5, height-.5);
		if(i % 50)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}
	cairo_set_source(c,white);
	for(i = 40; i < 360; i+=10) {
		double t = amplitude_to_time(w->la,i);
		if(t > span) continue;
		int x1 = round(width * (.5-.5*t/span));
		int x2 = round(width * (.5+.5*t/span));
		if(!(i % 50)) {
			char s[10];
			sprintf(s,"%d",abs(i));
			cairo_move_to(c,x1+font/4,height-font/2);
			cairo_show_text(c,s);
			cairo_move_to(c,x2+font/4,height-font/2);
			cairo_show_text(c,s);
		}
	}

	int old;
	struct processing_buffers *p = w->get_data(w,&old);

	if(p) {
		double span_time = p->period * span;

		double a = p->period - span_time;
		double b = p->period + span_time;

		draw_graph(a,b,c,p,w->amp_drawing_area);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke(c);
	}

	cairo_set_source(c,white);
	cairo_move_to(c,font/2,3*font/2);
	cairo_show_text(c,"Amplitude (deg)");

	cairo_destroy(c);

	return FALSE;
}

gboolean be_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	int width = w->be_drawing_area->allocation.width;
	int height = w->be_drawing_area->allocation.height;
	int font = width / 90;
	if(font < 12)
		font = 12;
	int i;

	c = gdk_cairo_create(widget->window);
	cairo_set_line_width(c,1);
	cairo_set_font_size(c,font);

	cairo_set_source(c,black);
	cairo_paint(c);

	for(i = -19; i < 20; i++) {
		int x = (20 + i) * width / 40;
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height-.5);
		if(i%5)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}
	cairo_set_source(c,white);
	for(i = -19; i < 20; i++) {
		int x = (20 + i) * width / 40;
		if(!(i%5)) {
			char s[10];
			sprintf(s,"%d",abs(i));
			cairo_move_to(c,x+font/4,height-font/2);
			cairo_show_text(c,s);
		}
	}

	int old;
	struct processing_buffers *p = w->get_data(w,&old);

	if(p && p->tic > 0) {
		double span = 0.02 * p->sample_rate;

		double a = p->tic - span;
		double b = p->tic + span;

		draw_graph(a,b,c,p,w->be_drawing_area);

		cairo_set_source(c,old?yellowish:gray);
		cairo_set_line_width(c,3);
		cairo_stroke(c);

		a = p->tic + p->period/2 - span;
		b = p->tic + p->period/2 + span;

		draw_graph(a,b,c,p,w->be_drawing_area);

		cairo_set_source(c,old?yellow:white);
		cairo_set_line_width(c,1);
		cairo_stroke(c);
	}

	cairo_set_source(c,white);
	cairo_move_to(c,font/2,3*font/2);
	cairo_show_text(c,"Beat error (ms)");

	cairo_destroy(c);

	return FALSE;
}

gboolean waveform_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	int width = w->be_drawing_area->allocation.width;
	int height = w->be_drawing_area->allocation.height;
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


	cairo_set_source(c,white);
	cairo_move_to(c,font/2,3*font/2);
	cairo_show_text(c,"Waveform");

	cairo_destroy(c);

	return FALSE;
}

void redraw(struct main_window *w)
{
	gtk_widget_queue_draw_area(w->output_drawing_area,0,0,w->output_drawing_area->allocation.width,w->output_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->amp_drawing_area,0,0,w->amp_drawing_area->allocation.width,w->amp_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->be_drawing_area,0,0,w->be_drawing_area->allocation.width,w->be_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->waveform_drawing_area,0,0,w->waveform_drawing_area->allocation.width,w->waveform_drawing_area->allocation.height);
}

void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
	char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));
	if(s) {
		int n;
		char *t;
		n = strtol(s,&t,10);
		if(*t || n < MIN_BPH || n > MAX_BPH) w->bph = 0;
		else w->bph = n;
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

	w->output_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->output_drawing_area),500,OUTPUT_WINDOW_HEIGHT);
	gtk_box_pack_start(GTK_BOX(vbox),w->output_drawing_area,FALSE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->output_drawing_area),"expose_event",
			(GtkSignalFunc)output_expose_event, w);
	gtk_widget_set_events(w->output_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->output_drawing_area);

	w->amp_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->amp_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox),w->amp_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->amp_drawing_area),"expose_event",
			(GtkSignalFunc)amp_expose_event, w);
	gtk_widget_set_events(w->amp_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->amp_drawing_area);

	w->be_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->be_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox),w->be_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->be_drawing_area),"expose_event",
			(GtkSignalFunc)be_expose_event, w);
	gtk_widget_set_events(w->be_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->be_drawing_area);

	w->waveform_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->waveform_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox),w->waveform_drawing_area,TRUE,TRUE,0);
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

struct interactive_w_data {
	struct processing_buffers *bfs;
	struct processing_buffers *old;
};

struct processing_buffers *int_get_data(struct main_window *w, int *old)
{
	struct interactive_w_data *d = w->data;
	struct processing_buffers *p = d->bfs;
	int i;
	for(i=0; i<NSTEPS; i++)
		if(p[i].period < 0) break;
	if(i && p[i-1].accept) {
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
	analyze_pa_data(p, w->bph);
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
	w.bph = 0;
	w.la = DEFAULT_LA;

	init_main_window(&w);

	g_timeout_add(round(1000*(sqrt(5)-1)/2),(GSourceFunc)refresh,&w);

	gtk_main();

	return 0;
}
/*
struct processing_buffers *file_get_data(struct main_window *w, int *old)
{
	struct processing_buffers *p = w->data;

	*old = 0;
	return p->accept ? p : NULL;
}

void file_recompute(struct main_window *w)
{
	struct processing_buffers *p = w->data;

	int err = compute_period(p,w->bph);
	if(err) {
		debug("---\n");
		p->accept = 0;
	} else {
		debug("%f +- %f\n",p->period,p->sigma);
		p->accept = acceptable(p);
	}
}

int run_on_file(char *filename)
{
	struct processing_buffers p;
	if(process_file(filename,&p)) return 1;

	struct main_window w;
	w.destroy = quit;
	w.data = &p;
	w.get_data = file_get_data;
	w.recompute = file_recompute;
	w.bph = 0;
	w.la = DEFAULT_LA;

	file_recompute(&w);

	init_main_window(&w);

	gtk_main();

	return 0;
}
*/
int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);
	initialize_palette();

//	if(argc == 1)
		return run_interactively();
//	else if(argc == 2 && argv[1][0] != '-')
//		return run_on_file(argv[1]);
//	else {
//		fprintf(stderr,"USAGE: %s [filename]\n",argv[0]);
//		return 1;
//	}
}
