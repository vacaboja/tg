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

float pa_buffers[2][PA_BUFF_SIZE];
int write_pointer = 0;
uint64_t timestamp = 0;
pthread_mutex_t audio_mutex;

int paudio_callback(const void *input_buffer,
			void *output_buffer,
			unsigned long frame_count,
			const PaStreamCallbackTimeInfo *time_info,
			PaStreamCallbackFlags status_flags,
			void *data)
{
	unsigned long i;
	long channels = (long)data;
	int wp = write_pointer;
	for(i=0; i < frame_count; i++) {
		if(channels == 1) {
			pa_buffers[0][wp] = ((float *)input_buffer)[i];
			pa_buffers[1][wp] = ((float *)input_buffer)[i];
		} else {
			pa_buffers[0][wp] = ((float *)input_buffer)[2*i];
			pa_buffers[1][wp] = ((float *)input_buffer)[2*i + 1];
		}
		if(wp < PA_BUFF_SIZE - 1) wp++;
		else wp = 0;
	}
	pthread_mutex_lock(&audio_mutex);
	write_pointer = wp;
	timestamp += frame_count;
	pthread_mutex_unlock(&audio_mutex);
	return 0;
}

int start_portaudio(int *nominal_sample_rate, double *real_sample_rate)
{
	PaStream *stream;

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
	err = Pa_OpenDefaultStream(&stream,channels,0,paFloat32,PA_SAMPLE_RATE,paFramesPerBufferUnspecified,paudio_callback,(void*)channels);
	if(err!=paNoError)
		goto error;

	err = Pa_StartStream(stream);
	if(err!=paNoError)
		goto error;

	const PaStreamInfo *info = Pa_GetStreamInfo(stream);
	*nominal_sample_rate = PA_SAMPLE_RATE;
	*real_sample_rate = info->sampleRate;
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

void fill_buffers(struct processing_buffers *p, int light)
{
	pthread_mutex_lock(&audio_mutex);
	uint64_t ts = timestamp;
	int wp = write_pointer;
	pthread_mutex_unlock(&audio_mutex);
	if(wp < 0 || wp >= PA_BUFF_SIZE) wp = 0;
	if(light) {
		if(wp % 2) wp--;
		ts /= 2;
	}
	int i;
	for(i=0; i<NSTEPS; i++) {
		int j,k;
		p[i].timestamp = ts;
		if(light) k = wp - 2*p[i].sample_count;
		else k = wp - p[i].sample_count;

		if(k < 0) k += PA_BUFF_SIZE;
		for(j=0; j < p[i].sample_count; j++) {
			p[i].samples[j] = pa_buffers[0][k] + pa_buffers[1][k];
			k += light ? 2 : 1;
			if(k >= PA_BUFF_SIZE) k -= PA_BUFF_SIZE;
		}
	}
}

int analyze_pa_data(struct processing_data *pd, int bph, double la, uint64_t events_from)
{
	struct processing_buffers *p = pd->buffers;
	fill_buffers(p, pd->is_light);

	int i;
	debug("\nSTART OF COMPUTATION CYCLE\n\n");
	for(i=0; i<NSTEPS; i++) {
		p[i].last_tic = pd->last_tic;
		p[i].events_from = events_from;
		process(&p[i], bph, la, pd->is_light);
		if( !p[i].ready ) break;
		debug("step %d : %f +- %f\n",i,p[i].period/p[i].sample_rate,p[i].sigma/p[i].sample_rate);
	}
	if(i) {
		pd->last_tic = p[i-1].last_tic;
		debug("%f +- %f\n",p[i-1].period/p[i-1].sample_rate,p[i-1].sigma/p[i-1].sample_rate);
	} else
		debug("---\n");
	return i;
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
