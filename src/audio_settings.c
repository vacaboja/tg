/*
 * settings.c
 *
 *  Created on: Apr 28, 2020
 *      Author: jlewis
 */


#include "audio.h"

#include <gtk/gtkHelper.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>
#include <string.h>



void handle_light(GtkToggleButton *b, struct main_window *w);
int sampleRateChange( struct main_window *w, int newSampleRate);
void handle_AudioInputChange (GtkComboBox *audioInputCombo, struct main_window *w);
void handle_SampleRateChange(GtkComboBox *Combo, struct main_window *w);

GtkWidget *audioSampleRateCombo;
GtkWidget *audioInputCombo;




static char* fillInputComboBox(char* audioInputStr, struct main_window *w){
	g_signal_handlers_disconnect_by_func(audioInputCombo, G_CALLBACK(handle_AudioInputChange), w);
	gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(audioInputCombo));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audioInputCombo), DEFAULT_AUDIOINPUTSTRING);
	int active = 0;
	int added  = 0;
	char * activeInputName = NULL;
	for (int n=0; n <= audio_num_inputs(w->audioInterfaceStr); n++) {
		const char * inputname = audio_InputDeviceName(w->audioInterfaceStr, n);
		if(inputname!=NULL){
			if(audio_devicename_supports_rate(w->audioInterfaceStr, inputname, -1))
			{
				gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audioInputCombo), inputname);
				added++;
				if (strcmp(audioInputStr, inputname) == 0){
					active= added;
					activeInputName=strdup(inputname);
				}
			}
		}
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(audioInputCombo), active);
	g_signal_connect (audioInputCombo, "changed", G_CALLBACK(handle_AudioInputChange), w);
	return activeInputName;
}

static void fillRatesComboBox(char*activeAudioInput, struct main_window *w){
	g_signal_handlers_disconnect_by_func(audioSampleRateCombo,  G_CALLBACK(handle_SampleRateChange), w);
	gtk_combo_box_text_remove_all (GTK_COMBO_BOX_TEXT(audioSampleRateCombo));
	//if(activeAudioInput && strcmp(activeAudioInput, DEFAULT_AUDIOINPUTSTRING)==0)
	//	activeAudioInput = NULL;
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(audioSampleRateCombo), DEFAULT_AUDIORATESTRING, DEFAULT_AUDIORATESTRING);

	int sampleRates[] = {48000, 44100, 32768, 24000, 22050, 16384, 11025};
	int activeSampleIndex = 0;
	int added = 0;
	for (int n=0; n < sizeof(sampleRates)/sizeof(sampleRates[0]); n++) {
		int sampleRate = sampleRates[n];
		if( audio_devicename_supports_rate(w->audioInterfaceStr, activeAudioInput, sampleRate)>=0 ){
			char sampleRateString[1024];
			sprintf(sampleRateString,"%d", sampleRate);
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(audioSampleRateCombo), sampleRateString, sampleRateString);
			added++;
			if (w->nominal_sr == sampleRate)
				activeSampleIndex=added;

		}
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(audioSampleRateCombo), activeSampleIndex); // Fill in value from settings
	g_signal_connect (audioSampleRateCombo, "changed", G_CALLBACK(handle_SampleRateChange), w);

}

void handle_AudioInterfaceChange(GtkComboBox *audioInterfaceCombo, struct main_window *w){
	char* audioInterfaceName = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(audioInterfaceCombo));
	terminate_portaudio();
	double real_sr;
	char *useAudioInterfaceName = audioInterfaceName;


	w->nominal_sr = USE_DEVICE_DEFAULT_AUDIORATE;
	start_portaudio(&w->nominal_sr, &real_sr, DEFAULT_AUDIOINPUTSTRING, useAudioInterfaceName, TRUE);

		// useAudioInterfaceName = DEFAULT_AUDIOINTERFACESTRING;



	g_free(w->audioInterfaceStr);
	w->audioInterfaceStr = g_strdup(audioInterfaceName);
	g_free(w->audioInputStr);
	w->audioInputStr = g_strdup(DEFAULT_AUDIOINPUTSTRING);
	save_config(w);

	char* audioInputStr = fillInputComboBox(DEFAULT_AUDIOINPUTSTRING, w);
	fillRatesComboBox(audioInputStr, w);
	g_free(audioInputStr);

	g_free(audioInterfaceName);
}

void handle_AudioInputChange (GtkComboBox *audioInputCombo, struct main_window *w){
	char* audioInputName = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(audioInputCombo));
	if(audioInputName){
		terminate_portaudio();
		double real_sr;
		char * useAudioInputName = audioInputName;
		w->nominal_sr = USE_DEVICE_DEFAULT_AUDIORATE;
		int nTry = 2;
		for(int i=0;i<nTry;i++){
			if(start_portaudio(&w->nominal_sr, &real_sr, useAudioInputName, w->audioInterfaceStr, i==nTry-1)==0){
				g_free(w->audioInputStr);	w->audioInputStr = g_strdup(useAudioInputName);
				fillRatesComboBox(w->audioInputStr, w);
				save_config(w);
				break;
			}
			if(i==1) useAudioInputName = DEFAULT_AUDIOINPUTSTRING;
		}
		g_free(audioInputName);
	}
}




void handle_SampleRateChange(GtkComboBox *Combo, struct main_window *w){
	char* sampleRateString = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(Combo));
	int sampleRate = PA_SAMPLE_RATE;
	if(sampleRateString != NULL){
		if( strcmp(sampleRateString,DEFAULT_AUDIORATESTRING)==0){
			sampleRate = USE_DEVICE_DEFAULT_AUDIORATE;
		}else{
			sscanf(sampleRateString, "%d", &sampleRate);
		}
		sampleRateChange(w, sampleRate);
		gchar id[1024];
		sprintf(id,"%d",w->nominal_sr);
		if(!gtk_combo_box_set_active_id (GTK_COMBO_BOX(Combo), id)){
			//add it on if not already there.
			gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(Combo), id, id);
			gtk_combo_box_set_active_id (GTK_COMBO_BOX(Combo), id);
		}
	}
}




/* Display the Settings dialog */
//Thanks to Rob Wahlstedt for this code ;)  https://github.com/wahlstedt/tg
void show_preferences(GtkButton *button, struct main_window *w) {
	GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT /* | GTK_DIALOG_MODAL*/;
	GtkWidget *dialog = gtk_dialog_new_with_buttons("Audio Settings",
										  GTK_WINDOW(w->window),
										  flags,
										//  ("Cancel"), GTK_RESPONSE_CANCEL,
										  "Done", GTK_RESPONSE_ACCEPT,
										  NULL);
	gtk_window_set_modal (GTK_WINDOW(dialog), FALSE);  //not working on Windows - it's still modal
	//gtk_window_set_transient_for (GTK_WINDOW(dialog), GTK_WINDOW(w->window));
	// gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE); // Non-resizable

	int yPos = 0;

	GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_container_set_border_width(GTK_CONTAINER(content_area), 5);
	GtkWidget *prefs_grid = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(prefs_grid), 6);
	gtk_grid_set_row_spacing(GTK_GRID(prefs_grid), 4);
	gtk_container_set_border_width(GTK_CONTAINER(prefs_grid), 10);
	gtk_widget_set_halign(prefs_grid, GTK_ALIGN_CENTER); // Keep grid centered in case user resizes window
	gtk_container_add(GTK_CONTAINER(content_area), prefs_grid); // Add the grid to the dialog


	//audio interface
	GtkWidget *interface_label = gtk_label_new("Audio interface:");
	gtk_widget_set_tooltip_text(interface_label, "What audio interface to connect to");
	gtk_widget_set_halign(interface_label, GTK_ALIGN_START); // Right aligned
	gtk_grid_attach(GTK_GRID(prefs_grid), interface_label, 0,yPos++,1,1);

	GtkWidget *audioInterfaceCombo = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audioInterfaceCombo), DEFAULT_AUDIOINTERFACESTRING);
	int activeInterface = 0;
	//char * activeInputName = NULL;
	int added = 0;
	for (int n=0; n <= audio_num_interfaces(); n++) {
		const char * interfacename = audio_interface_name(n);
		if(interfacename!=NULL){
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(audioInterfaceCombo), interfacename);
			added++;
			if (strcmp(w->audioInterfaceStr, interfacename) == 0){
				activeInterface=added;
				//activeInputName=strdup(interfacename);
			}

		}
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(audioInterfaceCombo), activeInterface); // Fill in value from settings
	gtk_grid_attach_next_to(GTK_GRID(prefs_grid), audioInterfaceCombo, interface_label, GTK_POS_RIGHT, 2, 1);
	g_signal_connect (audioInterfaceCombo, "changed", G_CALLBACK(handle_AudioInterfaceChange), w);

	//audio input
	GtkWidget *input_label = gtk_label_new("Audio input:");
	gtk_widget_set_tooltip_text(input_label, "What audio input to listen to");
	gtk_widget_set_halign(input_label, GTK_ALIGN_START); // Right aligned
	gtk_grid_attach(GTK_GRID(prefs_grid), input_label, 0,yPos++,1,1);


	audioInputCombo = gtk_combo_box_text_new();
	char* activeInputName = fillInputComboBox(w->audioInputStr, w);


	gtk_grid_attach_next_to(GTK_GRID(prefs_grid), audioInputCombo, input_label, GTK_POS_RIGHT, 2, 1);



    //sample rate
	GtkWidget  *sample_label = gtk_label_new("Sample Rate:");
	gtk_widget_set_tooltip_text(sample_label, "Higher gives better quality, but requires more computational power");
	gtk_widget_set_halign(sample_label, GTK_ALIGN_START); // Right aligned
	gtk_grid_attach(GTK_GRID(prefs_grid), sample_label, 0,yPos++,1,1);

	audioSampleRateCombo = gtk_combo_box_text_new();
	fillRatesComboBox(activeInputName, w);

	if(activeInputName!=NULL)
		free(activeInputName);
	gtk_grid_attach_next_to(GTK_GRID(prefs_grid), audioSampleRateCombo, sample_label, GTK_POS_RIGHT, 2, 1);



	//check boxes

    int xpos = 0; int xwidth = 2;


    gtk_grid_attach(GTK_GRID(prefs_grid),
					addCheckButton("Light algorithm",  w->is_light, G_CALLBACK(handle_light), w),
					xpos, yPos, xwidth, 1);





	gtk_widget_show_all(dialog);

	int res = gtk_dialog_run(GTK_DIALOG(dialog)); // Show the dialog

	if (res == GTK_RESPONSE_ACCEPT) {
		// Save the dialog data in the settings variable and then save it out to disk
		//w->conf.audio_input = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(input));
		//w->conf.rate_adjustment = atof(gtk_entry_get_text(GTK_ENTRY(adjustment)));
		//w->conf.precision_mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(cpu));
		//w->conf.dark_theme = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dark));

		//save_settings(&w->conf);
	}

	// Get rid of the dialog
	gtk_widget_destroy(dialog);
}
