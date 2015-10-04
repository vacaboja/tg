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

volatile float pa_buffers[2][PA_BUFF_SIZE];
volatile int write_pointer = 0;
volatile uint64_t timestamp = 0;

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

int analyze_pa_data(struct processing_buffers *p, int bph, uint64_t events_from)
{
	static uint64_t last_tic = 0;
	int wp = write_pointer;
	uint64_t ts = timestamp;
	if(wp < 0 || wp >= PA_BUFF_SIZE) wp = 0;
	int i;
	for(i=0; i<NSTEPS; i++) {
		int j,k;
		k = wp - p[i].sample_count;
		if(k < 0) k += PA_BUFF_SIZE;
		for(j=0; j < p[i].sample_count; j++) {
			p[i].samples[j] = pa_buffers[0][k] + pa_buffers[1][k];
			//p[i].samples[j] = pa_buffers[1][k];
			if(++k == PA_BUFF_SIZE) k = 0;
		}
	}
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
