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
#include "audio.h"
#include <portaudio.h>

/* Huge buffer of audio */
float pa_buffers[PA_BUFF_SIZE];
int write_pointer = 0;
uint64_t timestamp = 0;
pthread_mutex_t audio_mutex;

PaStream *stream = NULL;

/* Data for PA callback to use */
static struct callback_info {
	int 	channels;	//!< Number of channels
	//bool	light;		//!< Light algorithm in use, copy half data
} info;


//audio interfaces
int audio_num_interfaces(){
	return Pa_GetHostApiCount();
}

const char * audio_interface_name(PaHostApiIndex i){
	const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(i);
	return (hostInfo!=NULL)?hostInfo->name : NULL;
}

PaHostApiIndex getHostAPIindex(const char* hostAPIname){
	if(hostAPIname != NULL){
		for(PaHostApiIndex i = 0; i < audio_num_interfaces(); i++){
			const char* iname = audio_interface_name(i);
			if(iname && strcmp(hostAPIname, iname)==0)
				return i;
		}
	}
	return Pa_GetDefaultHostApi();
}




//audio inputs
int audio_num_inputs(const char* hostAPIname){
	const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(getHostAPIindex(hostAPIname));
	return (hostInfo)?hostInfo->deviceCount:0;
}

const char * audio_InputDeviceName(const char* hostAPIname, int hostDevIndex) {
	PaDeviceIndex devIndex = Pa_HostApiDeviceIndexToDeviceIndex(getHostAPIindex(hostAPIname), hostDevIndex);
	if(devIndex >= 0){
		const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(devIndex);
		if(deviceInfo)
			return deviceInfo->name;
	}
	return NULL;
}



static const char * filterDev( PaDeviceIndex devIndex, int nominal_sample_rate){
	const PaDeviceInfo *info = Pa_GetDeviceInfo(devIndex);
		if (info->maxInputChannels > 0){
			PaStreamParameters inputParameters;
			inputParameters.channelCount = 2; // Stereo
			inputParameters.sampleFormat = paFloat32;
			// inputParameters.suggestedLatency = ;
			inputParameters.hostApiSpecificStreamInfo = NULL;
			inputParameters.device = devIndex;
			if( nominal_sample_rate < 0   || Pa_IsFormatSupported( &inputParameters, NULL, nominal_sample_rate ))
				return info->name;
			debug("%d is not supported by dev %s \n", nominal_sample_rate, info->name);
		}
		return NULL;
	}
//filterHostDev

static gboolean audio_device_supports_rate(int devIndex, int nominal_sr){

	if(devIndex >= 0){
		return filterDev(devIndex, nominal_sr)!=NULL;
	}
	return FALSE;
}


/*
 *  returns hostDevIndex
*/
static PaDeviceIndex audio_deviceIndex(const char *hostAPIname, const char *hostdeviceName){
	PaHostApiIndex hostApiIndex = getHostAPIindex(hostAPIname);
	if (!hostdeviceName || !strcmp(hostdeviceName,DEFAULT_AUDIOINPUTSTRING)){
		const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(hostApiIndex);
		if(hostInfo!=NULL){
			return  hostInfo->defaultInputDevice;
		}
	}
	int nInputs = audio_num_inputs(hostAPIname);
	for(int hostDevIndex=0; hostDevIndex<nInputs; hostDevIndex++){
		const char *name = audio_InputDeviceName(hostAPIname, hostDevIndex);
		if(name && !strcmp(name, hostdeviceName))
			return Pa_HostApiDeviceIndexToDeviceIndex(hostApiIndex, hostDevIndex);
	}
	return -1;
}

gboolean audio_devicename_supports_rate(const char *hostAPIname, const char* hostDevName, int nominal_sr){
	int devIndex = audio_deviceIndex(hostAPIname, hostDevName);
	gboolean supports =  audio_device_supports_rate(devIndex, nominal_sr);
	 //debug("supports_rate %s %s %d %d %d \n", hostAPIname, hostDevName, devIndex, nominal_sr, supports);
	 return supports;
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

	pthread_mutex_lock(&audio_mutex);
	write_pointer = wp;
	timestamp += frame_count;
	pthread_mutex_unlock(&audio_mutex);
	return 0;
}


void resetBuffers(){
	pthread_mutex_lock(&audio_mutex);
		memset(pa_buffers, 0, sizeof(pa_buffers));
		write_pointer = 0;

		timestamp = 0;

	pthread_mutex_unlock(&audio_mutex);
}

int start_portaudio(int *nominal_sample_rate, double *real_sample_rate,
					char* hostdeviceName, char*hostAPIname,
					gboolean bShowError)
{

    debug("start_portaudio %d   %s   %s \n", *nominal_sample_rate, hostAPIname, hostdeviceName);


	if(pthread_mutex_init(&audio_mutex,NULL)) {
		error("Failed to setup audio mutex");
		return 1;
	}



	PaError err = Pa_Initialize();
	if(err!=paNoError)
		goto error;

	resetBuffers();


	PaStreamParameters inputParameters;
	inputParameters.channelCount = 2; // Stereo
	inputParameters.sampleFormat = paFloat32;
	// inputParameters.suggestedLatency = ;
	inputParameters.hostApiSpecificStreamInfo = NULL;
	inputParameters.device = Pa_GetDefaultInputDevice();

	debug("Default Input Device %d\n",inputParameters.device);




	PaDeviceIndex selectedDeviceIndex = audio_deviceIndex(hostAPIname, hostdeviceName);
	if(selectedDeviceIndex >= 0)
		inputParameters.device = selectedDeviceIndex;

	if(inputParameters.device == paNoDevice) {
			error("No default audio input device found");
			return 1;
	}

	const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(inputParameters.device);
	long long channels = deviceInfo->maxInputChannels;
	if(channels == 0) {
		error(" audio device has no input channels");
		return 1;
	}
	if(channels > 2) channels = 2;

	if(*nominal_sample_rate == USE_DEVICE_DEFAULT_AUDIORATE){
		*nominal_sample_rate = round(deviceInfo->defaultSampleRate);
		if(*nominal_sample_rate > MAX_PA_SAMPLE_RATE)
			*nominal_sample_rate = MAX_PA_SAMPLE_RATE;
		debug("Using Device defaultSampleRate %d\n", *nominal_sample_rate);
	}

	inputParameters.channelCount = info.channels = channels;

	if(inputParameters.device == Pa_GetDefaultInputDevice()){
		debug("Pa_OpenDefaultStream dev:%d channels:%d\n",inputParameters.device, inputParameters.channelCount);
		err = Pa_OpenDefaultStream(&stream,
				inputParameters.channelCount,0,
				inputParameters.sampleFormat,
				*nominal_sample_rate,
				paFramesPerBufferUnspecified,
				paudio_callback,
				&info);

	}else{


	//if(audioDeviceName != NULL){
	    debug("Pa_OpenStream dev:%d channels:%d\n",inputParameters.device, inputParameters.channelCount);
	    err = Pa_OpenStream(&stream,
							&inputParameters,
							NULL,							// outputParameters
							*nominal_sample_rate,
							paFramesPerBufferUnspecified,	// Frames per buffer
							paNoFlag,
							paudio_callback,
							&info
							);
	}
	/*if(err==paNoError)
		workingAudioDeviceName = g_strdup(audioDeviceName);
	*/
	if(err!=paNoError)
		goto error;

	err = Pa_StartStream(stream);
	if(err!=paNoError)
		goto error;

	const PaStreamInfo *info = Pa_GetStreamInfo(stream);
	*real_sample_rate = info->sampleRate;



	debug("sample rate: nominal = %d real = %f\n",*nominal_sample_rate,*real_sample_rate);

	return 0;

error:

	if(bShowError)
		error("Error opening audio input: %s", Pa_GetErrorText(err));
	else
		debug("Error attempting audio input: %s\n", Pa_GetErrorText(err));
	return 1;
}

int terminate_portaudio()
{

	debug("Closing portaudio\n");

	if(stream){
		PaError closeErr = Pa_CloseStream(stream);
		if(closeErr != paNoError) {
			error("Error closing audio: %s", Pa_GetErrorText(closeErr));
			return 1;
		}
	}
	stream = NULL;

	///free (lpf);  lpf = NULL; //this wasn't working as they may be being used in fill_buffers
	///free (hpf);  hpf = NULL;
	PaError err = Pa_Terminate();
	if(err != paNoError) {
		error("Error closing audio: %s", Pa_GetErrorText(err));
		return 1;
	}
	return 0;
}

uint64_t get_timestamp()
{
	pthread_mutex_lock(&audio_mutex);
	uint64_t ts = timestamp;
	pthread_mutex_unlock(&audio_mutex);
	return ts;
}

static void fill_buffers(struct processing_buffers *ps, int light)
{
	pthread_mutex_lock(&audio_mutex);
	uint64_t ts = timestamp;
	int wp = write_pointer;
	pthread_mutex_unlock(&audio_mutex);

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


