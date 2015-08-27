#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <math.h>
#include <complex.h>
#include <fftw3.h>
#include <stdint.h>
#include <portaudio.h>

#define FILTER_CUTOFF 3000
#define HIGHPASS 10

#define NSTEPS 3
#define FIRST_STEP 2
#define PA_SAMPLE_RATE 44100
#define PA_BUFF_SIZE (PA_SAMPLE_RATE << (NSTEPS + FIRST_STEP))

#define PA_SENTINEL 37

volatile float pa_buffers[2][PA_BUFF_SIZE];
volatile int write_pointer = 0;

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
		if(++write_pointer == PA_BUFF_SIZE) write_pointer = 0;
	}
    
        return 0;
}

int start_portaudio()
{
	PaStream *stream;

	PaError err = Pa_Initialize();
	if(err!=paNoError)
        	goto error;

	err = Pa_OpenDefaultStream(&stream,2,0,paFloat32,PA_SAMPLE_RATE,paFramesPerBufferUnspecified,paudio_callback,NULL);
	if(err!=paNoError)
		goto error;

	err = Pa_StartStream(stream);
	if(err!=paNoError)
		goto error;

	return 0;

error:
	fprintf(stderr,"Error opening audio input: %s\n", Pa_GetErrorText(err));
	return 1;
}

struct processing_buffers {
	int sample_rate;
	int sample_count;
	float *samples;
	fftwf_complex *fft;
	fftwf_plan plan_a, plan_b, plan_c, plan_d;
};

void setup_buffers(struct processing_buffers *b)
{
	b->samples = fftwf_malloc(2 * b->sample_count * sizeof(float));
	b->fft = malloc((b->sample_count + 1) * sizeof(fftwf_complex));
	b->plan_a = fftwf_plan_dft_r2c_1d(b->sample_count, b->samples, b->fft, FFTW_ESTIMATE);
	b->plan_b = fftwf_plan_dft_c2r_1d(b->sample_count, b->fft, b->samples, FFTW_ESTIMATE);
	b->plan_c = fftwf_plan_dft_r2c_1d(2 * b->sample_count, b->samples, b->fft, FFTW_ESTIMATE);
	b->plan_d = fftwf_plan_dft_c2r_1d(2 * b->sample_count, b->fft, b->samples, FFTW_ESTIMATE);
}

void compute_self_correlation(struct processing_buffers *b)
{
	int i;
	int first_fft_size = b->sample_count/2 + 1;

	fftwf_execute(b->plan_a);
	for(i=0; i < b->sample_count/2 + 1; i++) {
		if((uint64_t)b->sample_rate * i < (uint64_t)FILTER_CUTOFF * b->sample_count)
			b->fft[i] = 0;
	}
	fftwf_execute(b->plan_b);

	for(i=0; i < b->sample_count; i++)
		b->samples[i] = b->samples[i] * b->samples[i];

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

	double average = 0;
	for(i=0; i < b->sample_count; i++)
		average += b->samples[i];
	average /= b->sample_count;
	for(i=0; i < b->sample_count; i++)
		b->samples[i] -= average;

	fftwf_execute(b->plan_c);
	for(i=0; i < b->sample_count+1; i++) {
		if(  (uint64_t)b->sample_rate * i < (uint64_t)FILTER_CUTOFF * b->sample_count
				&&
		     (uint64_t)b->sample_rate * i > (uint64_t)HIGHPASS * b->sample_count  )
			b->fft[i] = b->fft[i] * conj(b->fft[i]);
		else
			b->fft[i] = 0;
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

int get_period(struct processing_buffers *b, double *period, double *sigma)
{
	double estimate = peak_detector(b, b->sample_rate / 6, b->sample_rate / 2);
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
		if(new_estimate == -1) return -1;
		new_estimate /= cycle;
		fprintf(stderr,"cycle = %d new_estimate = %f\n",cycle,new_estimate/44100);
		if(new_estimate < estimate - delta || new_estimate > estimate + delta) return -1;
		if(inf > b->sample_count / 3) {
			sum += new_estimate;
			sq_sum += new_estimate * new_estimate;
			count++;
		}
		cycle++;
	}
	if(count < 2) return -1;
	estimate = sum / count;
	*period = estimate / b->sample_rate;
	*sigma = sqrt((sq_sum - count * estimate * estimate)/ (count-1)) / b->sample_rate;
	return 0;
}

void save_debug(struct processing_buffers *p)
{
	SF_INFO out_info;
	SNDFILE *out_sfile;

	out_info.samplerate = p->sample_rate;
	out_info.channels = 1;
	out_info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

	out_sfile = sf_open("out.wav",SFM_WRITE,&out_info);
	if(!out_sfile) {
		fprintf(stderr,"Can not open out.wav\n");
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

	fprintf(stderr,"Written %d samples to out.wav\n",sf_write_float(out_sfile, p->samples, p->sample_count));

	sf_close(out_sfile);
}

int process_file(char *filename)
{
	SF_INFO sfinfo;
	SNDFILE *sfile;

	sfinfo.format = 0;

	sfile = sf_open(filename,SFM_READ,&sfinfo);
	if(!sfile) {
		fprintf(stderr,"Error opening file %s : %s\n",filename,sf_strerror(NULL));
		return 1;
	}

	if(sfinfo.channels != 1) {
		fprintf(stderr,"Channel count = %d != 1\n",sfinfo.channels);
		return 1;
	}

	struct processing_buffers b;
	
	b.sample_rate = sfinfo.samplerate;
	b.sample_count = sfinfo.frames;

	setup_buffers(&b);

	memset(b.samples,0,2 * b.sample_count * sizeof(float));

	if(sfinfo.frames != sf_read_float(sfile,b.samples,b.sample_count)) {
		fprintf(stderr,"Error in reading file %s\n",filename);
		return 1;
	}

	sf_close(sfile);

	compute_self_correlation(&b);

	double period = 0;
	double sigma = 0;
	int err = get_period(&b,&period,&sigma);
	if(err) printf("---\n");
	else printf("%f +- %f\n",period,sigma);

//	save_debug(&b);

	return 0;
}

void analyze_pa_data(struct processing_buffers *p)
{
	int wp = write_pointer;
	int i;
	for(i=0; i<NSTEPS; i++) {
		int j,k;
		memset(p[i].samples,0,2 * p[i].sample_count * sizeof(float));
		k = wp;
		for(j=0; j < p[i].sample_count; j++) {
			if(--k < 0) k = PA_BUFF_SIZE-1;
			p[i].samples[j] = pa_buffers[0][k] + pa_buffers[1][k];
		}
	}
	double period, sigma;
	for(i=0; i<NSTEPS; i++) {
		compute_self_correlation(&p[i]);

		if( get_period(&p[i],&period,&sigma) ) break;
			
		fprintf(stderr,"step %d : %f +- %f\n",i,period,sigma);
//		save_debug(&p[i]);
	}
	if(i && sigma < period / 10000)
		printf("%f +- %f\n",period,sigma);
	else
		printf("---\n");
}

int run_interactively()
{
	struct processing_buffers p[NSTEPS];
	int i;
	for(i=0; i<NSTEPS; i++) {
		p[i].sample_rate = PA_SAMPLE_RATE;
		p[i].sample_count = PA_SAMPLE_RATE * (1<<(i+FIRST_STEP));
		setup_buffers(&p[i]);
	}
	if(start_portaudio()) return 1;
	for(;;) {
		//sleep(1);
		analyze_pa_data(p);
	}
	return 0;
}

int main(int argc, char **argv)
{
	if(argc == 1)
		return run_interactively();
	else if(argc == 2 && argv[1][0] != '-')
		return process_file(argv[1]);
	else {
		fprintf(stderr,"USAGE: %s [filename]\n",argv[0]);
		return 1;
	}
}
