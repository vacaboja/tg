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

#include "tg.h"
#include <portaudio.h>

/* Huge buffer of audio */
float pa_buffers[PA_BUFF_SIZE];
unsigned int write_pointer = 0;
uint64_t timestamp = 0;
pthread_mutex_t audio_mutex;

/* Audio input stream object */
static PaStream *stream;

/** A biquadratic filter.
 * Saves the delay taps to allow the filter to continue across multiple calls. */
static struct biquad_filter {
	struct filter f;	//!< Filter coefficients, F(z) = a(z) / b(z)
	double        z1, z2;	//!< Delay taps
} audio_hpf;

/* Data for PA callback to use */
static struct callback_info {
	int 	channels;	//!< Number of channels
	bool	light;		//!< Light algorithm in use, copy half data
} info;

/* Initialize audio filter */
static void init_audio_hpf(struct biquad_filter *filter, double cutoff, double sample_rate)
{
	make_hp(&filter->f, cutoff/sample_rate);
	filter->z1 = 0.0;
	filter->z2 = 0.0;
}

/* Apply a biquadratic filter to data.  The delay values are updated in f, so
 * that it is possible to process data in chunks using multiple calls.
 */
static void apply_biquad(struct biquad_filter *f, float *data, unsigned int count)
{
	unsigned int i;
	double z1 = f->z1, z2 = f->z2;
	for(i=0; i<count; i++) {
		double in = data[i];
		double out = in * f->f.a0 + z1;
		z1 = in * f->f.a1 + z2 - f->f.b1 * out;
		z2 = in * f->f.a2 - f->f.b2 * out;
		data[i] = out;
	}
	f->z1 = z1;
	f->z2 = z2;
}

static int paudio_callback(const void *input_buffer,
			   void *output_buffer,
			   unsigned long frame_count,
			   const PaStreamCallbackTimeInfo *time_info,
			   PaStreamCallbackFlags status_flags,
			   void *data)
{
	UNUSED(output_buffer);
	UNUSED(time_info);
	UNUSED(status_flags);
	const float *input_samples = (const float*)input_buffer;
	unsigned long i;
	const struct callback_info *info = data;
	unsigned wp = write_pointer;

	if (info->light) {
		static bool even = true;
		/* Copy every other sample.  It would be much more efficient to
		 * just drop the sample rate if the sound hardware supports it.
		 * This would also avoid the aliasing effects that this simple
		 * decimation without a low-pass filter causes.  */
		if(info->channels == 1) {
			for(i = even ? 0 : 1; i < frame_count; i += 2) {
				pa_buffers[wp++] = input_samples[i];
				if (wp >= PA_BUFF_SIZE) wp -= PA_BUFF_SIZE;
			}
		} else {
			for(i = even ? 0 : 2; i < frame_count*2; i += 4) {
				pa_buffers[wp++] = input_samples[i] + input_samples[i+1];
				if (wp >= PA_BUFF_SIZE) wp -= PA_BUFF_SIZE;
			}
		}
		/* Keep track if we have processed an even number of frames, so
		 * we know if we should drop the 1st or 2nd frame next callback. */
		if(frame_count % 2) even = !even;
	} else {
		const unsigned len = MIN(frame_count, PA_BUFF_SIZE - wp);
		if(info->channels == 1) {
			memcpy(pa_buffers + wp, input_samples, len * sizeof(*pa_buffers));
			if(len < frame_count)
				memcpy(pa_buffers, input_samples + len, (frame_count - len) * sizeof(*pa_buffers));
		} else {
			for(i = 0; i < len; i++)
				pa_buffers[wp + i] = input_samples[2u*i] + input_samples[2u*i + 1u];
			if(len < frame_count)
				for(i = len; i < frame_count; i++)
					pa_buffers[i - len] = input_samples[2u*i] + input_samples[2u*i + 1u];
		}
		wp = (wp + frame_count) % PA_BUFF_SIZE;
	}

	/* Apply HPF to new data */
	if(write_pointer < wp) {
		apply_biquad(&audio_hpf, pa_buffers + write_pointer, wp - write_pointer);
	} else {
		apply_biquad(&audio_hpf, pa_buffers + write_pointer, PA_BUFF_SIZE - write_pointer);
		apply_biquad(&audio_hpf, pa_buffers, wp);
	}

	pthread_mutex_lock(&audio_mutex);
	write_pointer = wp;
	timestamp += frame_count;
	pthread_mutex_unlock(&audio_mutex);
	return 0;
}

int start_portaudio(int *nominal_sample_rate, double *real_sample_rate, bool light)
{
	if(pthread_mutex_init(&audio_mutex,NULL)) {
		error("Failed to setup audio mutex");
		return 1;
	}

	PaError err = Pa_Initialize();
	if(err!=paNoError)
		goto error;

#ifdef DEBUG
	if(testing) {
		*nominal_sample_rate = PA_SAMPLE_RATE;
		*real_sample_rate = PA_SAMPLE_RATE;
		goto end;
	}
#endif

	PaDeviceIndex default_input = Pa_GetDefaultInputDevice();
	if(default_input == paNoDevice) {
		error("No default audio input device found");
		return 1;
	}
	long channels = Pa_GetDeviceInfo(default_input)->maxInputChannels;
	if(channels == 0) {
		error("Default audio device has no input channels");
		return 1;
	}
	if(channels > 2) channels = 2;
	info.channels = channels;
	info.light = light;
	err = Pa_OpenDefaultStream(&stream,channels,0,paFloat32,PA_SAMPLE_RATE,paFramesPerBufferUnspecified,paudio_callback,&info);
	if(err!=paNoError)
		goto error;

	const PaStreamInfo *info = Pa_GetStreamInfo(stream);
	*nominal_sample_rate = PA_SAMPLE_RATE;
	*real_sample_rate = info->sampleRate;

	init_audio_hpf(&audio_hpf, FILTER_CUTOFF, PA_SAMPLE_RATE / (light ? 2 : 1));

	err = Pa_StartStream(stream);
	if(err!=paNoError)
		goto error;
#ifdef DEBUG
end:
#endif
	debug("sample rate: nominal = %d real = %f\n",*nominal_sample_rate,*real_sample_rate);

	return 0;

error:
	error("Error opening audio input: %s", Pa_GetErrorText(err));
	return 1;
}

int terminate_portaudio()
{
	debug("Closing portaudio\n");
	PaError err = Pa_Terminate();
	if(err != paNoError) {
		error("Error closing audio: %s", Pa_GetErrorText(err));
		return 1;
	}
	return 0;
}

uint64_t get_timestamp(int light)
{
	pthread_mutex_lock(&audio_mutex);
	uint64_t ts = light ? timestamp / 2 : timestamp;
	pthread_mutex_unlock(&audio_mutex);
	return ts;
}

void fill_buffers(struct processing_buffers *ps, int light)
{
	pthread_mutex_lock(&audio_mutex);
	uint64_t ts = timestamp;
	int wp = write_pointer;
	pthread_mutex_unlock(&audio_mutex);

	if(light)
		ts /= 2;

	int i;
	for(i = 0; i < NSTEPS; i++) {
		ps[i].timestamp = ts;

		int start = wp - ps[i].sample_count;
		if (start < 0) start += PA_BUFF_SIZE;
		int len = MIN((unsigned)ps[i].sample_count, PA_BUFF_SIZE - start);
		memcpy(ps[i].samples, pa_buffers + start, len * sizeof(*pa_buffers));
		if (len < ps[i].sample_count)
			memcpy(ps[i].samples + len, pa_buffers, (ps[i].sample_count - len) * sizeof(*pa_buffers));
	}
}

/* Returns if buffer was processed ok */
bool analyze_pa_data(struct processing_data *pd, int step, int bph, double la, uint64_t events_from)
{
	struct processing_buffers *p = &pd->buffers[step];

	p->last_tic = pd->last_tic;
	p->events_from = events_from;
	process(p, bph, la, pd->is_light);
	debug("step %d : %f +- %f\n", step, p->period/p->sample_rate, p->sigma/p->sample_rate);

	return p->ready;
}

int analyze_pa_data_cal(struct processing_data *pd, struct calibration_data *cd)
{
	struct processing_buffers *p = pd->buffers;
	fill_buffers(p, pd->is_light);

	int i,j;
	debug("\nSTART OF CALIBRATION CYCLE\n\n");
	for(j=0; p[j].sample_count < 2*p[j].sample_rate; j++);
	for(i=0; i+j<NSTEPS-1; i++)
		if(test_cal(&p[i+j]))
			return i ? i+j : 0;
	if(process_cal(&p[NSTEPS-1], cd))
		return NSTEPS-1;
	return NSTEPS;
}

/** Change to light mode
 *
 * Call to enable or disable light mode.  Changing the mode will empty the audio
 * buffer.  Nothing will happen if the mode doesn't actually change.
 *
 * @param light True for light mode, false for normal
 * @param sample_rate Nominal sampling rate.  Should take into account any
 * downsampling done in light mode
 */
void set_audio_light(bool light, int sample_rate)
{
	if(info.light != light) {
		Pa_StopStream(stream);
		pthread_mutex_lock(&audio_mutex);

		info.light = light;
		memset(pa_buffers, 0, sizeof(pa_buffers));
		write_pointer = 0;
		timestamp = 0;

		pthread_mutex_unlock(&audio_mutex);

		init_audio_hpf(&audio_hpf, FILTER_CUTOFF, sample_rate);
		PaError err = Pa_StartStream(stream);
		if(err != paNoError)
			error("Error re-starting audio input: %s", Pa_GetErrorText(err));
	}
}
