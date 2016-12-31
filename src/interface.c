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

#ifdef DEBUG
int testing = 0;
#endif

int preset_bph[] = PRESET_BPH;

void print_debug(char *format,...)
{
	va_list args;
	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);
}

void error(char *format,...)
{
	char s[100];
	va_list args;

	va_start(args,format);
	int size = vsnprintf(s,100,format,args);
	va_end(args);

	char *t;
	if(size < 100) {
		t = s;
	} else {
		t = alloca(size+1);
		va_start(args,format);
		vsnprintf(t,size+1,format,args);
		va_end(args);
	}

	fprintf(stderr,"%s\n",t);

#ifdef DEBUG
	if(testing) return;
#endif

	GtkWidget *dialog = gtk_message_dialog_new(NULL,0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",t);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
}

void recompute(struct main_window *w)
{
	w->computer_timeout = 0;
	lock_computer(w->computer);
	w->computer->bph = w->bph;
	w->computer->la = w->la;
	w->computer->calibrate = w->calibrate;
	if(w->computer->recompute >= 0)
		w->computer->recompute = 1;
	unlock_computer(w->computer);
}

guint kick_computer(struct main_window *w)
{
	w->computer_timeout++;
	if(w->calibrate && w->computer_timeout < 10) {
		return TRUE;
	} else {
		recompute(w);
		return TRUE;
	}
}

void refresh_results(struct main_window *w)
{
	w->snapshots[0]->bph = w->bph;
	w->snapshots[0]->la = w->la;
	w->snapshots[0]->cal = w->cal;
	compute_results(w->snapshots[0]);
}

void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
	char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));
	if(s) {
		int bph;
		char *t;
		int n = (int)strtol(s,&t,10);
		if(*t || n < MIN_BPH || n > MAX_BPH) bph = 0;
		else bph = n;
		g_free(s);
		w->bph = bph;
		refresh_results(w);
		redraw_op(w->op);
	}
}

void handle_la_change(GtkSpinButton *b, struct main_window *w)
{
	double la = gtk_spin_button_get_value(b);
	if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
	w->la = la;
	refresh_results(w);
	redraw_op(w->op);
}

void handle_cal_change(GtkSpinButton *b, struct main_window *w)
{
	int cal = gtk_spin_button_get_value(b);;
	w->cal = cal;
	refresh_results(w);
	redraw_op(w->op);
}

gboolean output_cal(GtkSpinButton *spin, gpointer data)
{
	GtkAdjustment *adj;
	gchar *text;
	int value;

	adj = gtk_spin_button_get_adjustment (spin);
	value = (int)gtk_adjustment_get_value (adj);
	text = g_strdup_printf ("%c%d.%d", value < 0 ? '-' : '+', abs(value)/10, abs(value)%10);
	gtk_entry_set_text (GTK_ENTRY (spin), text);
	g_free (text);

	return TRUE;
}

gboolean input_cal(GtkSpinButton *spin, double *val, gpointer data)
{
	double x = 0;
	sscanf(gtk_entry_get_text (GTK_ENTRY (spin)), "%lf", &x);
	int n = round(x*10);
	if(n < MIN_CAL) n = MIN_CAL;
	if(n > MAX_CAL) n = MAX_CAL;
	*val = n;

	return TRUE;
}

void handle_calibrate(GtkToggleButton *b, struct main_window *w)
{
	int button_state = gtk_toggle_button_get_active(b) == TRUE;
	if(button_state != w->calibrate) {
		w->calibrate = button_state;
		recompute(w);
	}
}

guint close_main_window(struct main_window *w)
{
	debug("Closing main window\n");
	gtk_widget_destroy(w->window);
	gtk_main_quit();
	return FALSE;
}

void computer_quit(void *w)
{
	gdk_threads_add_idle((GSourceFunc)close_main_window,w);
}

gboolean quit(struct main_window *w)
{
	lock_computer(w->computer);
	w->computer->recompute = -1;
	w->computer->callback = computer_quit;
	w->computer->callback_data = w;
	unlock_computer(w->computer);
	return FALSE;
}

gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer w)
{
	debug("Received delete event\n");
	quit((struct main_window *)w);
	return TRUE;
}

/* Set up the main window and populate with widgets */
void init_main_window(struct main_window *w)
{
	w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_container_set_border_width(GTK_CONTAINER(w->window), 10); // Border around the window
	g_signal_connect(w->window, "delete_event", G_CALLBACK(delete_event), w);

	gtk_window_set_title(GTK_WINDOW(w->window), PROGRAM_NAME " " VERSION);

	GtkWidget *vbox = gtk_vbox_new(FALSE, 10); // Replaced by GtkGrid in GTK+ 3.2
	gtk_container_add(GTK_CONTAINER(w->window), vbox);
	gtk_widget_show(vbox);

	GtkWidget *hbox = gtk_hbox_new(FALSE, 10); // Replaced by GtkGrid in GTK+ 3.2
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
	gtk_widget_show(hbox);

	// BPH label
	GtkWidget *label = gtk_label_new("bph");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	// BPH combo box
	GtkWidget *bph_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_box_pack_start(GTK_BOX(hbox), bph_combo_box, FALSE, TRUE, 0);
	// Fill in pre-defined values
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bph_combo_box), "guess");
	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		char s[100];
		sprintf(s,"%d", preset_bph[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(bph_combo_box), s);
		if(w->bph == preset_bph[i]) current = i+1;
	}
	if(current || w->bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(bph_combo_box), current);
	else {
		char s[32];
		sprintf(s,"%d",w->bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(bph_combo_box)));
		gtk_entry_set_text(e,s);
	}
	g_signal_connect (bph_combo_box, "changed", G_CALLBACK(handle_bph_change), w);
	gtk_widget_show(bph_combo_box);

	// Lift angle label
	label = gtk_label_new("lift angle");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	// Lift angle spin button
	GtkWidget *la_spin_button = gtk_spin_button_new_with_range(MIN_LA, MAX_LA, 1);
	gtk_box_pack_start(GTK_BOX(hbox), la_spin_button, FALSE, TRUE, 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(la_spin_button), w->la);
	g_signal_connect(la_spin_button, "value_changed", G_CALLBACK(handle_la_change), w);
	gtk_widget_show(la_spin_button);

	// Lift angle label
	label = gtk_label_new("cal");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	// Calibration spin button
	w->cal_spin_button = gtk_spin_button_new_with_range(MIN_CAL, MAX_CAL, 1);
	gtk_box_pack_start(GTK_BOX(hbox), w->cal_spin_button, FALSE, TRUE, 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), w->cal);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w->cal_spin_button), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(w->cal_spin_button), 6);
	g_signal_connect(w->cal_spin_button, "value_changed", G_CALLBACK(handle_cal_change), w);
	g_signal_connect(w->cal_spin_button, "output", G_CALLBACK(output_cal), NULL);
	g_signal_connect(w->cal_spin_button, "input", G_CALLBACK(input_cal), NULL);
	gtk_widget_show(w->cal_spin_button);

	// CALIBRATE button
	GtkWidget *cal_button = gtk_toggle_button_new_with_label("Calibrate");
	gtk_box_pack_end(GTK_BOX(hbox), cal_button, FALSE, FALSE, 0);
	g_signal_connect(cal_button, "toggled", G_CALLBACK(handle_calibrate), w);
	gtk_widget_show(cal_button);

	gtk_box_pack_start(GTK_BOX(vbox), w->op->panel, TRUE, TRUE, 0);
	gtk_widget_show(w->op->panel);

	gtk_window_maximize(GTK_WINDOW(w->window));
	gtk_widget_show(w->window);
	gtk_window_set_focus(GTK_WINDOW(w->window), NULL);
}

guint save_on_change_timer(struct main_window *w)
{
	save_on_change(w);
	return TRUE;
}

guint refresh(struct main_window *w)
{
	lock_computer(w->computer);
	struct snapshot *s = w->computer->curr;
	if(s) {
		double trace_centering = w->snapshots[0]->trace_centering;
		snapshot_destroy(w->snapshots[0]);
		w->snapshots[0] = s;
		w->computer->curr = NULL;
		s->trace_centering = trace_centering;
		if(w->computer->clear_trace)
			memset(s->events,0,EVENTS_COUNT*sizeof(uint64_t));
		if(s->calibrate && s->cal_state == 1 && s->cal_result != w->cal) {
			w->cal = s->cal_result;
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), s->cal_result);
		}
	}
	unlock_computer(w->computer);
	w->snapshots[0]->bph = w->bph;
	w->snapshots[0]->la = w->la;
	w->snapshots[0]->cal = w->cal;
	compute_results(w->snapshots[0]);
	w->op->computer = w->current_snapshot ? NULL : w->computer;
	w->op->snst = w->snapshots[w->current_snapshot];
	redraw_op(w->op);
	return FALSE;
}

void computer_callback(void *w)
{
	gdk_threads_add_idle((GSourceFunc)refresh,w);
}

int run_interface()
{
	int nominal_sr;
	double real_sr;

	if(start_portaudio(&nominal_sr, &real_sr)) return 1;

	struct main_window w;

	w.cal = MIN_CAL - 1;
	w.bph = 0;
	w.la = DEFAULT_LA;
	w.calibrate = 0;

	load_config(&w);

	if(w.la < MIN_LA || w.la > MAX_LA) w.la = DEFAULT_LA;
	if(w.bph < MIN_BPH || w.bph > MAX_BPH) w.bph = 0;
	if(w.cal < MIN_CAL || w.cal > MAX_CAL)
		w.cal = (real_sr - nominal_sr) * (3600*24) / nominal_sr;

	w.snapshots = malloc(sizeof(struct snapshot));
	w.current_snapshot = 0;
	w.computer_timeout = 0;

	w.computer = start_computer(nominal_sr, w.bph, w.la, w.cal);
	if(!w.computer) return 2;
	w.computer->callback = computer_callback;
	w.computer->callback_data = &w;

	w.snapshots[0] = w.computer->curr;
	w.computer->curr = NULL;
	compute_results(w.snapshots[0]);

	w.op = init_output_panel(w.computer, w.snapshots[0]);

	init_main_window(&w);

	g_timeout_add_full(G_PRIORITY_LOW,100,(GSourceFunc)kick_computer,&w,NULL);
	g_timeout_add_full(G_PRIORITY_LOW,10000,(GSourceFunc)save_on_change_timer,&w,NULL);
#ifdef DEBUG
	if(testing)
		g_timeout_add_full(G_PRIORITY_LOW,3000,(GSourceFunc)quit,&w,NULL);
#endif

	gtk_main();
	debug("Main loop has terminated\n");

	save_config(&w);

	// We leak the processing buffers, program is terminating anyway
	return 10*terminate_portaudio();
}

int main(int argc, char **argv)
{
#if !GLIB_CHECK_VERSION(2,31,0)
	g_thread_init(NULL);
#endif
	gdk_threads_init();
	gdk_threads_enter();

	gtk_init(&argc, &argv);
#ifdef DEBUG
	if(argc > 1 && !strcmp("test",argv[1]))
		testing = 1;
#endif
	initialize_palette();

	int ret = run_interface();
	debug("Interface exited with status %d\n",ret);

	gdk_threads_leave();

	return ret;
}
