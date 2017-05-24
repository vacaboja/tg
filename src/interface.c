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
	w->active_snapshot->bph = w->bph;
	w->active_snapshot->la = w->la;
	w->active_snapshot->cal = w->cal;
	compute_results(w->active_snapshot);
}

void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
	if(!w->controls_active) return;
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
		gtk_widget_queue_draw(w->notebook);
	}
}

void handle_la_change(GtkSpinButton *b, struct main_window *w)
{
	if(!w->controls_active) return;
	double la = gtk_spin_button_get_value(b);
	if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
	w->la = la;
	refresh_results(w);
	gtk_widget_queue_draw(w->notebook);
}

void handle_cal_change(GtkSpinButton *b, struct main_window *w)
{
	if(!w->controls_active) return;
	int cal = gtk_spin_button_get_value(b);
	w->cal = cal;
	refresh_results(w);
	gtk_widget_queue_draw(w->notebook);
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

void handle_calibrate(GtkCheckMenuItem *b, struct main_window *w)
{
	int button_state = gtk_check_menu_item_get_active(b) == TRUE;
	if(button_state != w->calibrate) {
		w->calibrate = button_state;
		gtk_widget_set_sensitive(w->snapshot_button, !button_state);
		recompute(w);
	}
}

guint close_main_window(struct main_window *w)
{
	debug("Closing main window\n");
	gtk_widget_destroy(w->window);
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

void controls_active(struct main_window *w, int active)
{
	w->controls_active = active;
	gtk_widget_set_sensitive(w->bph_combo_box, active);
	gtk_widget_set_sensitive(w->la_spin_button, active);
	gtk_widget_set_sensitive(w->cal_spin_button, active);
	gtk_widget_set_sensitive(w->cal_button, active);
	if(active) {
		gtk_widget_show(w->snapshot_button);
		gtk_widget_hide(w->snapshot_name);
	} else {
		gtk_widget_hide(w->snapshot_button);
		gtk_widget_show(w->snapshot_name);
	}
}

void handle_tab_closed(GtkNotebook *nbk, GtkWidget *panel, guint x, void *p)
{
	if(gtk_notebook_get_n_pages(nbk) == 1) {
		gtk_notebook_set_show_tabs(GTK_NOTEBOOK(nbk), FALSE);
		gtk_notebook_set_show_border(GTK_NOTEBOOK(nbk), FALSE);
	}
	// Now, are we sure that we are not going to segfault?
	struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");
	if(op) op_destroy(op);
}

void handle_tab_changed(GtkNotebook *nbk, GtkWidget *panel, guint x, struct main_window *w)
{
	// These are NULL for the Real Time tab
	struct output_panel *op = g_object_get_data(G_OBJECT(panel), "op-pointer");
	GtkLabel *label = g_object_get_data(G_OBJECT(panel), "tab-label");

	controls_active(w, !op);

	int bph, cal;
	double la;
	if(op) {
		gtk_entry_set_text(GTK_ENTRY(w->snapshot_name_entry), gtk_label_get_text(label));
		bph = op->snst->bph;
		cal = op->snst->cal;
		la = op->snst->la;
	} else {
		bph = w->bph;
		cal = w->cal;
		la = w->la;
	}
	// you never know where this snapshot has been loaded from...
	if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
	if(bph < MIN_BPH || bph > MAX_BPH) bph = 0;
	if(cal < MIN_CAL || cal > MAX_CAL) cal = 0;

	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		if(bph == preset_bph[i]) {
			current = i+1;
			break;
		}
	}
	if(current || bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), current);
	else {
		char s[32];
		sprintf(s,"%d",bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box)));
		gtk_entry_set_text(e,s);
	}

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), la);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), cal);
}

void handle_close_tab(GtkButton *b, struct output_panel *p)
{
	gtk_widget_destroy(p->panel);
}

void handle_name_change(GtkEntry *e, struct main_window *w)
{
	int p = gtk_notebook_get_current_page(GTK_NOTEBOOK(w->notebook));
	GtkWidget *panel = gtk_notebook_get_nth_page(GTK_NOTEBOOK(w->notebook), p);
	GtkLabel *label = g_object_get_data(G_OBJECT(panel), "tab-label");
	gtk_label_set_text(label, gtk_entry_get_text(e));
}

GtkWidget *make_tab_label(char *s, struct output_panel *panel_to_close)
{
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	GtkWidget *label = gtk_label_new(s);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	if(panel_to_close) {
		GtkWidget *image = gtk_image_new_from_icon_name("window-close-symbolic", GTK_ICON_SIZE_MENU);
		GtkWidget *button = gtk_button_new();
		gtk_button_set_image(GTK_BUTTON(button), image);
		gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
		g_signal_connect(button, "clicked", G_CALLBACK(handle_close_tab), panel_to_close);
		gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(panel_to_close->panel), "op-pointer", panel_to_close);
		g_object_set_data(G_OBJECT(panel_to_close->panel), "tab-label", label);
	}

	gtk_widget_show_all(hbox);

	return hbox;
}

void handle_snapshot(GtkButton *b, struct main_window *w)
{
	if(w->active_snapshot->calibrate) return;

	struct snapshot *s = snapshot_clone(w->active_snapshot);
	s->timestamp = get_timestamp();
	struct output_panel *op = init_output_panel(NULL, s, 5);
	GtkWidget *label = make_tab_label("Snapshot", op);
	gtk_widget_show_all(op->panel);

	op_set_border(w->active_panel, 5);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_append_page(GTK_NOTEBOOK(w->notebook), op->panel, label);
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(w->notebook), op->panel, TRUE);
}

/* Set up the main window and populate with widgets */
void init_main_window(struct main_window *w)
{
	w->window = gtk_application_window_new(w->app);

	gtk_widget_set_size_request(w->window, 950, 700);

	gtk_container_set_border_width(GTK_CONTAINER(w->window), 10);
	g_signal_connect(w->window, "delete_event", G_CALLBACK(delete_event), w);

	gtk_window_set_title(GTK_WINDOW(w->window), PROGRAM_NAME " " VERSION);

	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_add(GTK_CONTAINER(w->window), vbox);

	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	// BPH label
	GtkWidget *label = gtk_label_new("bph");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	// BPH combo box
	w->bph_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_box_pack_start(GTK_BOX(hbox), w->bph_combo_box, FALSE, FALSE, 0);
	// Fill in pre-defined values
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box), "guess");
	int i,current = 0;
	for(i = 0; preset_bph[i]; i++) {
		char s[100];
		sprintf(s,"%d", preset_bph[i]);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box), s);
		if(w->bph == preset_bph[i]) current = i+1;
	}
	if(current || w->bph == 0)
		gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), current);
	else {
		char s[32];
		sprintf(s,"%d",w->bph);
		GtkEntry *e = GTK_ENTRY(gtk_bin_get_child(GTK_BIN(w->bph_combo_box)));
		gtk_entry_set_text(e,s);
	}
	g_signal_connect (w->bph_combo_box, "changed", G_CALLBACK(handle_bph_change), w);

	// Lift angle label
	label = gtk_label_new("lift angle");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	// Lift angle spin button
	w->la_spin_button = gtk_spin_button_new_with_range(MIN_LA, MAX_LA, 1);
	gtk_box_pack_start(GTK_BOX(hbox), w->la_spin_button, FALSE, FALSE, 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), w->la);
	g_signal_connect(w->la_spin_button, "value_changed", G_CALLBACK(handle_la_change), w);

	// Calibration label
	label = gtk_label_new("cal");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	// Calibration spin button
	w->cal_spin_button = gtk_spin_button_new_with_range(MIN_CAL, MAX_CAL, 1);
	gtk_box_pack_start(GTK_BOX(hbox), w->cal_spin_button, FALSE, FALSE, 0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), w->cal);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(w->cal_spin_button), FALSE);
	gtk_entry_set_width_chars(GTK_ENTRY(w->cal_spin_button), 6);
	g_signal_connect(w->cal_spin_button, "value_changed", G_CALLBACK(handle_cal_change), w);
	g_signal_connect(w->cal_spin_button, "output", G_CALLBACK(output_cal), NULL);
	g_signal_connect(w->cal_spin_button, "input", G_CALLBACK(input_cal), NULL);

	// Is there a more elegant way?
	GtkWidget *empty = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), empty, TRUE, FALSE, 0);

	// Snapshot button
	w->snapshot_button = gtk_button_new_with_label("Take Snapshot");
	gtk_box_pack_start(GTK_BOX(hbox), w->snapshot_button, FALSE, FALSE, 0);
	g_signal_connect(w->snapshot_button, "clicked", G_CALLBACK(handle_snapshot), w);

	// Snapshot name field
	GtkWidget *name_label = gtk_label_new("Current snapshot");
	w->snapshot_name_entry = gtk_entry_new();
	w->snapshot_name = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(w->snapshot_name), name_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(w->snapshot_name), w->snapshot_name_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), w->snapshot_name, FALSE, FALSE, 0);
	g_signal_connect(w->snapshot_name_entry, "changed", G_CALLBACK(handle_name_change), w);

	empty = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), empty, TRUE, FALSE, 0);

	// Command menu
	GtkWidget *command_menu = gtk_menu_new();
	GtkWidget *command_menu_button = gtk_menu_button_new();
	GtkWidget *image = gtk_image_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image(GTK_BUTTON(command_menu_button), image);
	g_object_set(G_OBJECT(command_menu_button), "direction", GTK_ARROW_DOWN, NULL);
	g_object_set(G_OBJECT(command_menu), "halign", GTK_ALIGN_END, NULL);
	gtk_menu_button_set_popup(GTK_MENU_BUTTON(command_menu_button), command_menu);
	gtk_box_pack_end(GTK_BOX(hbox), command_menu_button, FALSE, FALSE, 0);

	// Calibrate checkbox
	w->cal_button = gtk_check_menu_item_new_with_label("Calibrate");
	gtk_menu_shell_append(GTK_MENU_SHELL(command_menu), w->cal_button);
	g_signal_connect(w->cal_button, "toggled", G_CALLBACK(handle_calibrate), w);

	gtk_widget_show_all(command_menu);

	// The tabs' container
	w->notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(vbox), w->notebook, TRUE, TRUE, 0);
	gtk_notebook_set_scrollable(GTK_NOTEBOOK(w->notebook), TRUE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(w->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(w->notebook), FALSE);
	g_signal_connect(w->notebook, "page-removed", G_CALLBACK(handle_tab_closed), NULL);
	g_signal_connect_after(w->notebook, "switch-page", G_CALLBACK(handle_tab_changed), w);

	// The main tab
	GtkWidget *tab_label = make_tab_label("Real time", NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(w->notebook), w->active_panel->panel, tab_label);
	gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(w->notebook), w->active_panel->panel, TRUE);

	gtk_window_maximize(GTK_WINDOW(w->window));
	gtk_widget_show_all(w->window);
	gtk_widget_hide(w->snapshot_name);
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
		double trace_centering = w->active_snapshot->trace_centering;
		snapshot_destroy(w->active_snapshot);
		w->active_snapshot = s;
		w->computer->curr = NULL;
		s->trace_centering = trace_centering;
		if(w->computer->clear_trace && !s->calibrate)
			memset(s->events,0,EVENTS_COUNT*sizeof(uint64_t));
		if(s->calibrate && s->cal_state == 1 && s->cal_result != w->cal) {
			w->cal = s->cal_result;
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->cal_spin_button), s->cal_result);
		}
	}
	unlock_computer(w->computer);
	refresh_results(w);
	op_set_snapshot(w->active_panel, w->active_snapshot);
	gtk_widget_queue_draw(w->notebook);
	return FALSE;
}

void computer_callback(void *w)
{
	gdk_threads_add_idle((GSourceFunc)refresh,w);
}

int start_interface(GtkApplication* app, void *p)
{
	struct main_window *w = g_object_get_data(G_OBJECT(app), "main-window");
	if(w) {
		debug("Application already active\n");
		gtk_window_present(GTK_WINDOW(w->window));
		return 0;
	}

	int nominal_sr;
	double real_sr;

	initialize_palette();
	if(start_portaudio(&nominal_sr, &real_sr)) return 1;

	w = malloc(sizeof(struct main_window));

	w->app = app;

	w->controls_active = 1;
	w->cal = MIN_CAL - 1;
	w->bph = 0;
	w->la = DEFAULT_LA;
	w->calibrate = 0;

	load_config(w);

	if(w->la < MIN_LA || w->la > MAX_LA) w->la = DEFAULT_LA;
	if(w->bph < MIN_BPH || w->bph > MAX_BPH) w->bph = 0;
	if(w->cal < MIN_CAL || w->cal > MAX_CAL)
		w->cal = (real_sr - nominal_sr) * (3600*24) / nominal_sr;

	w->computer_timeout = 0;

	w->computer = start_computer(nominal_sr, w->bph, w->la, w->cal);
	if(!w->computer) return 2;
	w->computer->callback = computer_callback;
	w->computer->callback_data = w;

	w->active_snapshot = w->computer->curr;
	w->computer->curr = NULL;
	compute_results(w->active_snapshot);

	w->active_panel = init_output_panel(w->computer, w->active_snapshot, 0);

	init_main_window(w);

	g_timeout_add_full(G_PRIORITY_LOW,100,(GSourceFunc)kick_computer,w,NULL);
	g_timeout_add_full(G_PRIORITY_LOW,10000,(GSourceFunc)save_on_change_timer,w,NULL);
#ifdef DEBUG
	if(testing)
		g_timeout_add_full(G_PRIORITY_LOW,3000,(GSourceFunc)quit,w,NULL);
#endif

	g_object_set_data(G_OBJECT(app), "main-window", w);

	return 0;
}

void on_shutdown(GApplication *app, void *p)
{
	debug("Main loop has terminated\n");
	save_config(g_object_get_data(G_OBJECT(app), "main-window"));
	terminate_portaudio();
	// We leak the main_window structure
}

int main(int argc, char **argv)
{
#ifdef DEBUG
	if(argc > 1 && !strcmp("test",argv[1]))
		testing = 1;
#endif

	GtkApplication *app = gtk_application_new ("li.ciovil.tg", G_APPLICATION_FLAGS_NONE);
	g_signal_connect (app, "activate", G_CALLBACK (start_interface), NULL);
	g_signal_connect (app, "shutdown", G_CALLBACK (on_shutdown), NULL);
	int ret = g_application_run (G_APPLICATION (app), argc, argv);
	g_object_unref (app);

	debug("Interface exited with status %d\n",ret);

	return ret;
}
