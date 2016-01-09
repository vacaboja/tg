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
#include <gtk/gtk.h>

static GKeyFile *key_file; // Keep this object alive for the duration of the app

void load_settings(struct Settings *conf) {
	
	GError *err = NULL;
	key_file = g_key_file_new();
	
	if(!g_key_file_load_from_file(key_file,
								  "tg.ini",
								  G_KEY_FILE_KEEP_COMMENTS |
								  G_KEY_FILE_KEEP_TRANSLATIONS,
								  &err))
	{
		g_message("Couldn't load settings file: %s", err->message);
		g_error_free (err);
	}
	else {
		conf->audio_input = g_key_file_get_string(key_file, "main", "audio_input", &err);
		conf->rate_adjustment = g_key_file_get_double(key_file, "main", "rate_adjustment", &err);
		conf->precision_mode = g_key_file_get_boolean(key_file, "main", "precision_mode", &err);
		conf->ticks = g_key_file_get_boolean(key_file, "main", "ticks", &err);
		conf->dark_theme = g_key_file_get_boolean(key_file, "ui", "dark_theme", &err);
		conf->window_width = g_key_file_get_integer(key_file, "ui", "window_width", &err);
		conf->window_height = g_key_file_get_integer(key_file, "ui", "window_height", &err);
		conf->pane_pos = g_key_file_get_integer(key_file, "ui", "pane_pos", &err);
	}
}

void save_settings(struct Settings *conf) {
	g_key_file_set_string(key_file, "main", "audio_input", conf->audio_input);
	g_key_file_set_double(key_file, "main", "rate_adjustment", conf->rate_adjustment);
	g_key_file_set_boolean(key_file, "main", "precision_mode", conf->precision_mode);
	g_key_file_set_boolean(key_file, "main", "ticks", conf->ticks);
	g_key_file_set_boolean(key_file, "ui", "dark_theme", conf->dark_theme);
	g_key_file_set_integer(key_file, "ui", "window_width", conf->window_width);
	g_key_file_set_integer(key_file, "ui", "window_height", conf->window_height);
	g_key_file_set_integer(key_file, "ui", "pane_pos", conf->pane_pos);
	
	GError *err = NULL;
	if (!g_key_file_save_to_file(key_file, "tg.ini", &err)) {
		g_message("Failed to save settings: %s", err->message);
		g_error_free (err);
	}
}
