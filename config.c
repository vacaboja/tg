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

#if !GLIB_CHECK_VERSION(2,40,0)
gboolean
g_key_file_save_to_file (GKeyFile     *key_file,
                         const gchar  *filename,
                         GError      **error)
{
  gchar *contents;
  gboolean success;
  gsize length;

  g_return_val_if_fail (key_file != NULL, FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  contents = g_key_file_to_data (key_file, &length, NULL);
  g_assert (contents != NULL);

  success = g_file_set_contents (filename, contents, length, error);
  g_free (contents);

  return success;
}
#endif

#define g_key_file_get_int g_key_file_get_integer
#define g_key_file_set_int g_key_file_set_integer // the devil may take glib

void load_config(struct main_window *w)
{
	w->config_file = g_key_file_new();
	w->config_file_name = g_build_filename(g_get_user_config_dir(), CONFIG_FILE_NAME, NULL);
	w->conf_data = malloc(sizeof(struct conf_data));
	debug("Config: loading configuration file %s\n", w->config_file_name);
	gboolean ret = g_key_file_load_from_file(w->config_file, w->config_file_name, G_KEY_FILE_KEEP_COMMENTS, NULL);
	if(!ret) {
		debug("Config: failed to load config file");
		return;
	}

#define LOAD(NAME,PLACE,TYPE) \
	{ \
		GError *e = NULL; \
		TYPE val = g_key_file_get_ ## TYPE (w->config_file, "tg", #NAME, &e); \
		if(e) { \
			debug("Config: error loading field " #NAME "\n"); \
			g_error_free(e); \
		} else { \
			w -> PLACE = val; \
			w -> conf_data -> PLACE = val; \
		} \
	}

	CONFIG_FIELDS(LOAD);
}

void save_config(struct main_window *w)
{
	debug("Config: saving configuration file\n");

	g_key_file_set_string(w->config_file, "tg", "version", VERSION);

#define SAVE(NAME,PLACE,TYPE) \
	g_key_file_set_ ## TYPE (w->config_file, "tg", #NAME, w -> PLACE); \
	w -> conf_data -> PLACE = w -> PLACE;

	CONFIG_FIELDS(SAVE);

#ifdef DEBUG
	GError *ge = NULL;
	g_key_file_save_to_file(w->config_file, w->config_file_name, &ge);

	if(ge) {
		debug("Config: failed to save config file: %s\n",ge->message);
		g_error_free(ge);
		if(testing) exit(1);
	}
#else
	g_key_file_save_to_file(w->config_file, w->config_file_name, NULL);
#endif
}

void save_on_change(struct main_window *w)
{
#define CHANGED(NAME,PLACE,TYPE) \
	if(w -> PLACE != w -> conf_data -> PLACE) { \
		save_config(w); \
		return; \
	}

	CONFIG_FIELDS(CHANGED);
}
