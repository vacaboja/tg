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

volatile float pa_buffers[2][PA_BUFF_SIZE]; // Buffers to store the audio sample data
volatile int write_pointer = 0; // Current write position in the buffers
volatile uint64_t timestamp = 0; // Running timestamp

/* Callback function that consume audio in response to requests from an active PortAudio stream */
int paudio_callback(const void *input_buffer,
			void *output_buffer,
			unsigned long frame_count,
			const PaStreamCallbackTimeInfo *time_info,
			PaStreamCallbackFlags status_flags,
			void *data)
{
	unsigned long i;
    // Copy the sample data to pa_buffers[] (Mac mini gets 512 samples on each callback)
	for(i=0; i < frame_count; i++) {
		pa_buffers[0][write_pointer] = ((float *)input_buffer)[2*i];
		pa_buffers[1][write_pointer] = ((float *)input_buffer)[2*i + 1];
		if(write_pointer < PA_BUFF_SIZE - 1) write_pointer++;
        else write_pointer = 0; // Wrap over when reaching the buffer end
        timestamp++; // Increase timestamp for each frame TODO: Move outside loop?
	}
	return 0;
}

/* Set up PA to continuously sample audio and store in buffers */
int start_portaudio(int *nominal_sample_rate, double *real_sample_rate)
{
	PaStream *stream;

	PaStream **x = malloc(sizeof(PaStream*));

	PaError err = Pa_Initialize();
	if(err!=paNoError)
		goto error;

    // Open an audio stream with 2 input channels & 0 output channels with an unspecified buffer size
	err = Pa_OpenDefaultStream(&stream,2,0,paFloat32,PA_SAMPLE_RATE,paFramesPerBufferUnspecified,paudio_callback,x);
	*x = stream;
	if(err!=paNoError)
		goto error;

	err = Pa_StartStream(stream);
	if(err!=paNoError)
		goto error;

	const PaStreamInfo *info = Pa_GetStreamInfo(stream);
#ifdef LIGHT
	*nominal_sample_rate = PA_SAMPLE_RATE / 2;
	*real_sample_rate = info->sampleRate / 2;
#else
	*nominal_sample_rate = PA_SAMPLE_RATE;
    *real_sample_rate = info->sampleRate; // Actual sample rate, reported by PortAudio
#endif
	debug("sample rate: nominal = %d real = %f\n",*nominal_sample_rate,*real_sample_rate);

	return 0;

error:
	error("Error opening audio input: %s", Pa_GetErrorText(err));
	return 1;
}

int analyze_pa_data(struct processing_buffers *p, int bph, uint64_t events_from)
{
	static uint64_t last_tic = 0;
    int wp = write_pointer; // Current write position in the buffers
	uint64_t ts = timestamp;
    if(wp < 0 || wp >= PA_BUFF_SIZE) wp = 0; // Sanity check for the buffer pointer
#ifdef LIGHT
    if(wp % 2) wp--; // Make sure pointer is even.
	ts /= 2;
#endif
	int i;
	for(i=0; i<NSTEPS; i++) {
		int j,k;
#ifdef LIGHT
		k = wp - 2*p[i].sample_count;
#else
		k = wp - p[i].sample_count;
#endif
		if(k < 0) k += PA_BUFF_SIZE;
		for(j=0; j < p[i].sample_count; j++) {
            p[i].samples[j] = pa_buffers[0][k] + pa_buffers[1][k]; // Merge stereo into mono(?)
#ifdef LIGHT
			k += 2;
#else
			k++;
#endif
			if(k >= PA_BUFF_SIZE) k -= PA_BUFF_SIZE;
		}
	}
	debug("\nSTART OF COMPUTATION CYCLE\n\n");
	for(i=0; i<NSTEPS; i++) {
		p[i].timestamp = ts;
		p[i].last_tic = last_tic;
		p[i].events_from = events_from;
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
