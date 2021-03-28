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
#include <errno.h>

/* Huge buffer of audio */
float *pa_buffers;
unsigned int pa_buffer_size;
unsigned int write_pointer = 0;
uint64_t timestamp = 0;
pthread_mutex_t audio_mutex;

/** A biquadratic filter.
 * Saves the delay taps to allow the filter to continue across multiple calls. */
struct biquad_filter {
	struct filter f;	//!< Filter coefficients, F(z) = a(z) / b(z)
	double        z1, z2;	//!< Delay taps
};

/** Data for PA callback to use */
struct callback_info {
	int 	channels;	//!< Number of channels
	bool	light;		//!< Light algorithm in use, copy half data
	struct biquad_filter hpf; //!< High-pass filter run in callback
};

/** Static object for audio device state.
 * There are calls that need this from the audio callback thread, the GUI thread, and
 * the computer thread.  Having each thread pass it in correctly would be really hard.
 * It's better to maintain it in one place here in the audio code.  Lacking class scope
 * in C, we'll have to settle for static global scope.  We only support once device at a
 * time, so not supporting multiuple audio contexts isn't much of a drawback.
 * */
static struct audio_context {
	PaStream *stream;	//!< Audio input stream object
	int device;  		//!< PortAudio device ID number
	int sample_rate;	//!< Requested sample rate (actual may differ)
	double real_sample_rate;//!< Real rate as returned by PA
	unsigned num_devices;	//!< Number of audio devices for current driver
	struct audio_device *devices;  //!< Cached audio device info
	//! Data callback will read, need to take care when modifying so as not to race.
	struct callback_info info;
} actx = {
	.device = -1,
};

/** Return effective sample rate.
 * This takes into account the half speed decimation enabled by light mode */
static inline double effective_sr(void)
{
	return actx.real_sample_rate / (actx.info.light ? 2 : 1);
}

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
				if (wp >= pa_buffer_size) wp -= pa_buffer_size;
			}
		} else {
			for(i = even ? 0 : 2; i < frame_count*2; i += 4) {
				pa_buffers[wp++] = input_samples[i] + input_samples[i+1];
				if (wp >= pa_buffer_size) wp -= pa_buffer_size;
			}
		}
		/* Keep track if we have processed an even number of frames, so
		 * we know if we should drop the 1st or 2nd frame next callback. */
		if(frame_count % 2) even = !even;
	} else {
		const unsigned len = MIN(frame_count, pa_buffer_size - wp);
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
		wp = (wp + frame_count) % pa_buffer_size;
	}

	/* Apply HPF to new data */
	struct biquad_filter *f = (struct biquad_filter *)&info->hpf;
	if(write_pointer < wp) {
		apply_biquad(f, pa_buffers + write_pointer, wp - write_pointer);
	} else {
		apply_biquad(f, pa_buffers + write_pointer, pa_buffer_size - write_pointer);
		apply_biquad(f, pa_buffers, wp);
	}

	pthread_mutex_lock(&audio_mutex);
	write_pointer = wp;
	timestamp += frame_count;
	pthread_mutex_unlock(&audio_mutex);
	return 0;
}

static PaError open_stream(PaDeviceIndex index, unsigned int rate, bool light, PaStream **stream)
{
	PaError err;

	long channels = Pa_GetDeviceInfo(index)->maxInputChannels;
	if(channels == 0) {
		error("Default audio device has no input channels");
		return paInvalidChannelCount;
	}
	if(channels > 2) channels = 2;
	actx.info.channels = channels;
	actx.info.light = light;

	err = Pa_OpenStream(stream,
			    &(PaStreamParameters){
			            .device = index,
				    .channelCount = channels,
				    .sampleFormat = paFloat32,
				    .suggestedLatency = Pa_GetDeviceInfo(index)->defaultHighInputLatency,
			    },
		            NULL,
			    rate,
			    paFramesPerBufferUnspecified,
			    paNoFlag,
			    paudio_callback,
			    &actx.info);
	return err;
}

/** Select audio device and enable recording.
 *
 * This will select `device` to be the active audio device and capture at the
 * rate provided in `*nominal_sr`.  If `*normal_sr` is zero, then a default rate
 * is selected.
 *
 * It is safe to call if the device and rate are unchanged.  This will be
 * detected and nothing will be done.
 *
 * Light mode will use simple decimation to cut the sample rate in half.  The
 * values in nominal and real sr do not reflect this.
 *
 * @param device Device number, index of device from get_audio_devices() list
 * @param[in,out] normal_sr Desired rate, or zero for default.  Rate used on return.
 * @param[out] real_sr Actual exact rate received, might be different than nominal_sr.
 * @param[light] light Use light mode (halve normal_sr)
 * @returns zero or one on success or negative error code.  1 indicates no
 * change in device or rate was needed.
 */
int set_audio_device(int device, int *nominal_sr, double *real_sr, bool light)
{
	PaError err;

	// FIXME: Use a list of rates and pick the first supported rate
	if(*nominal_sr == 0)
		*nominal_sr = PA_SAMPLE_RATE;

	if(actx.device == device && actx.sample_rate == *nominal_sr) {
		if(real_sr) *real_sr = actx.real_sample_rate;
		return 1; // Already using this device at this rate
	}

	if(actx.device != -1) {
		// Stop current device
		Pa_StopStream(actx.stream);
		Pa_CloseStream(actx.stream);
		actx.stream = NULL;
		actx.device = -1;
	}

	actx.sample_rate = *nominal_sr;

	// Start new one.  It seems it doesn't succeed on the first try sometimes.
	unsigned int n;
	for(n = 5; n; n--) {
		debug("Open device %d at %d Hz with %d tries left\n", device, actx.sample_rate, n);
		err = open_stream(device, actx.sample_rate, light, &actx.stream);
		if (err == paNoError)
			break;
		if (err != paDeviceUnavailable)
			goto error;
		usleep(500000);
	}
	if(!n)
		goto error;
	actx.real_sample_rate = Pa_GetStreamInfo(actx.stream)->sampleRate;

	init_audio_hpf(&actx.info.hpf, FILTER_CUTOFF, effective_sr());

	/* Allocate larger buffer if needed */
	const size_t buffer_size = actx.sample_rate << (NSTEPS + FIRST_STEP);
	if(pa_buffer_size < buffer_size) {
		if(pa_buffers) free(pa_buffers);
		pa_buffers = calloc(buffer_size, sizeof(*pa_buffers));
		if(!pa_buffers) {
			err = paInsufficientMemory;
			goto error;
		}
		pa_buffer_size = buffer_size;
	}

	err = Pa_StartStream(actx.stream);
	if(err != paNoError) {
		Pa_CloseStream(actx.stream);
		goto error;
	}

	/* Return sample rates used */
	*nominal_sr = actx.sample_rate;
	if(real_sr)
		*real_sr = actx.real_sample_rate;

	actx.device = device;
	return 0;

error:
	actx.stream = NULL;
	actx.device = -1;
	actx.sample_rate = 0;
	actx.real_sample_rate = 0.0;

	const struct PaDeviceInfo* devinfo = Pa_GetDeviceInfo(device);
	const char *err_str = Pa_GetErrorText(err);
	error("Error opening audio device '%s' at %d Hz: %s", devinfo->name, *nominal_sr, err_str);
	return err;
}

/** Return current audio device.
 *
 * @return Index of current audio device, or -1 if none is active.
 */
int get_audio_device(void)
{
	return actx.device;
}

/** Get list of devices.
 *
 * @param[out] devices Static list of devices.
 * @return Number of devices or negative on error.
 */
int get_audio_devices(const struct audio_device **devices)
{
	const struct audio_device* devs = actx.devices;
	*devices = devs;
	return actx.num_devices;
}

static bool check_audio_rate(int device, int rate)
{
	const long channels = Pa_GetDeviceInfo(device)->maxInputChannels;
	const PaStreamParameters params = {
		.device = device,
		.channelCount = channels > 2 ? 2 : channels,
		.sampleFormat = paFloat32,
	};

	return paFormatIsSupported == Pa_IsFormatSupported(&params, NULL, rate);
}


static int scan_audio_devices(void)
{
	const PaDeviceIndex default_input = Pa_GetDefaultInputDevice();
	const int n = Pa_GetDeviceCount();
	static const int rate_list[] = AUDIO_RATES;

	if (actx.devices) free(actx.devices);
	actx.devices = calloc(n, sizeof(*actx.devices));
	if (!actx.devices)
		return -ENOMEM;

	int i;
	for (i = 0; i < n; i++) {
		const struct PaDeviceInfo* devinfo = Pa_GetDeviceInfo(i);
		debug("Device %2d: %2d %s%s\n", i, devinfo->maxInputChannels, devinfo->name, i == default_input ? " (default)" : "");
		actx.devices[i].name = devinfo->name;
		actx.devices[i].good = devinfo->maxInputChannels > 0;
		actx.devices[i].isdefault = i == default_input;
		actx.devices[i].rates = 0;
		if (actx.devices[i].good) {
			unsigned r;
			for (r = 0; r < ARRAY_SIZE(rate_list); r++)
				if (check_audio_rate(i, rate_list[r]))
					actx.devices[i].rates |= 1 << r;
		}
	}
	actx.num_devices = n;

	return n;
}

/** Start audio system.
 *
 * This will start the recording stream.  Call this first before any other audio
 * functions, as it initialize PortAudio and fills in the device list.
 *
 * A sample rate of 0 will select the default sample rate.
 *
 * On error, audio is NOT running.
 *
 * The distinction between the nominal and real sample rate is somewhat ill-defined.
 * Nothing uses real sample rate yet.
 *
 * @param[in,out] normal_sample_rate The rate in Hz to use, or 0 for default.  Returns
 * actual rate selected.
 * @param[out] real_sample_rate The exact rate used.
 * @param light Use light mode (decimate to half supplied rate).
 * @returns 0 on success, 1 on error.
 *
 */
int start_portaudio(int *nominal_sample_rate, double *real_sample_rate, bool light)
{
	if(pthread_mutex_init(&audio_mutex,NULL)) {
		error("Failed to setup audio mutex");
		return 1;
	}

	PaError err = Pa_Initialize();
	if(err!=paNoError) {
		error("Error initializing PortAudio: %s", Pa_GetErrorText(err));
		goto error;
	}

#ifdef DEBUG
	if(testing) {
		*nominal_sample_rate = PA_SAMPLE_RATE;
		*real_sample_rate = PA_SAMPLE_RATE;
		goto end;
	}
#endif

	if(scan_audio_devices() < 0) {
		error("Unable to query audio devices");
		// Maybe default audio device will work anyway?
	}

	PaDeviceIndex default_input = Pa_GetDefaultInputDevice();
	if(default_input == paNoDevice) {
		error("No default audio input device found");
		goto error;
	}

	err = set_audio_device(default_input, nominal_sample_rate, real_sample_rate, light);
	if(err!=paNoError && err!=1)
		goto error;

#ifdef DEBUG
end:
#endif
	debug("sample rate: nominal = %d real = %f\n",*nominal_sample_rate,*real_sample_rate);

	return 0;

error:
	error("Unable to start audio");
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

uint64_t get_timestamp()
{
	pthread_mutex_lock(&audio_mutex);
	uint64_t ts = actx.info.light ? timestamp / 2 : timestamp;
	pthread_mutex_unlock(&audio_mutex);
	return ts;
}

void fill_buffers(struct processing_buffers *ps)
{
	pthread_mutex_lock(&audio_mutex);
	uint64_t ts = timestamp / (actx.info.light ? 2 : 1);
	int wp = write_pointer;
	pthread_mutex_unlock(&audio_mutex);

	int i;
	for(i = 0; i < NSTEPS; i++) {
		ps[i].timestamp = ts;

		int start = wp - ps[i].sample_count;
		if (start < 0) start += pa_buffer_size;
		int len = MIN((unsigned)ps[i].sample_count, pa_buffer_size - start);
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
	fill_buffers(p);

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
 * buffer.  Nothing will happen if the mode doesn't actually change.  Audio is
 * downsampled by 2 in light mode.
 *
 * @param light True for light mode, false for normal mode
 */
void set_audio_light(bool light)
{
	if(actx.info.light != light) {
		Pa_StopStream(actx.stream);
		pthread_mutex_lock(&audio_mutex);

		actx.info.light = light;
		memset(pa_buffers, 0, sizeof(*pa_buffers) * pa_buffer_size);
		write_pointer = 0;
		timestamp = 0;

		pthread_mutex_unlock(&audio_mutex);

		init_audio_hpf(&actx.info.hpf, FILTER_CUTOFF, effective_sr());
		PaError err = Pa_StartStream(actx.stream);
		if(err != paNoError)
			error("Error re-starting audio input: %s", Pa_GetErrorText(err));
	}
}
