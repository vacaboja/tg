/*
 * audio.h
 *
 *  Created on: May 19, 2020
 *      Author: jlewis
 */

#ifndef SRC_AUDIO_H_
#define SRC_AUDIO_H_

#include "tg.h"
#include <portaudio.h>



struct calibration_data;

/* audio.c */
struct processing_data {
	struct processing_buffers *buffers;
	uint64_t last_tic;
	int is_light;
};

void audio_setFiltersFreq(double lpfFreq, double hpfFreq);
int start_portaudio(int *nominal_sample_rate, double *real_sample_rate, char* audioDeviceName, char* audioInterfaceStr,
						gboolean bShowError);
int terminate_portaudio();
void audio_setHPF(double hpfCutOffFreq);
void  audio_setLPF( double lpfCutOffFreq);
uint64_t get_timestamp();
int analyze_pa_data(struct processing_data *pd, int bph, double la, uint64_t events_from);
int analyze_pa_data_cal(struct processing_data *pd, struct calibration_data *cd);
void markFreshData_audio();
int audio_nValidInputs(char* hostAPIname, int nominal_sample_rate);

//interfaces
int audio_num_interfaces();
const char * audio_interface_name(PaHostApiIndex i);

//devices
int 			audio_num_inputs(const char* hostAPIname);
const char * 	audio_InputDeviceName(const char* hostAPIname, int hostDevIndex);
gboolean 		audio_devicename_supports_rate(const char *hostAPIname, const char* hostDevName, int nominal_sr);
int 			audio_HostDeviceIndex(char* hostAPIname, const char* audioDeviceName, int nominal_sample_rate);

gboolean restartPortAudio();

#endif /* SRC_AUDIO_H_ */
