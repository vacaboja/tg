#include "tg.h"

int preset_bph[] = PRESET_BPH;

void debug(char *format,...)
{
	va_list args;
	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);
}

void error(char *format,...)
{
	va_list args;
	va_start(args,format);
	vfprintf(stderr,format,args);
	va_end(args);
}

struct main_window {
	GtkWidget *window;
	GtkWidget *bph_combo_box;
	GtkWidget *la_spin_button;
	GtkWidget *output_drawing_area;
	GtkWidget *amp_drawing_area;
	GtkWidget *tic_drawing_area;
	GtkWidget *toc_drawing_area;
	GtkWidget *waveform_drawing_area;
	GtkWidget *paperstrip_drawing_area;

	void (*destroy)(GtkWidget *widget, gpointer data);
	struct processing_buffers *(*get_data)(struct main_window *w, int *old);
	void (*recompute)(struct main_window *w);

	int bph;
	int guessed_bph;
	int last_bph;
	double la;

	uint64_t *events;
	int events_wp;
	uint64_t events_from;

	int signal;

	void *data;
};

cairo_pattern_t *black,*white,*red,*green,*gray,*blue,*yellow,*yellowish,*magenta;

void define_color(cairo_pattern_t **gc,double r,double g,double b)
{
	*gc = cairo_pattern_create_rgb(r,g,b);
}

void initialize_palette()
{
	define_color(&black,0,0,0);
	define_color(&white,1,1,1);
	define_color(&red,1,0,0);
	define_color(&green,0,0.8,0);
	define_color(&gray,0.5,0.5,0.5);
	define_color(&blue,0,0,1);
	define_color(&yellow,1,1,0);
	define_color(&yellowish,0.5,0.5,0);
	define_color(&magenta,1,0,1);
}

gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	return FALSE;
}

void draw_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	int width = da->allocation.width;
	int height = da->allocation.height;

	int n;

	int first = 1;
	for(n=0; n<2*width; n++) {
		int i = n < width ? n : 2*width - 1 - n;
		int j = round(a + i * (b-a) / width);
		double y;

		if(j < 0 || j >= p->period || p->waveform[j] <= 0) y = 0;
		else y = p->waveform[j] * 0.4 / p->waveform_max;

		int k = round(y*height);
		if(n < width) k = -k;

		if(first) {
			cairo_move_to(c,i+.5,height/2+k+.5);
			first = 0;
		} else
			cairo_line_to(c,i+.5,height/2+k+.5);
	}
}

void draw_amp_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	int width = da->allocation.width;
	int height = da->allocation.height;

	int i;
	float max = 0;

	int ai = round(a);
	int bi = 1+round(b);
	if(ai < 0) ai = 0;
	if(bi > p->sample_count) bi = p->sample_count;
	int qq = 0;
	for(i=ai; i<bi; i++) {
		if(p->samples_sc[i] > max) {
			max = p->samples_sc[i];
			qq = i;
		}
	}

	debug("+++ max = %f (%d %d %d)\n",max,ai,qq,bi);

	int first = 1;
	for(i=0; i<width; i++) {
		if( round(a + i*(b-a)/width) != round(a + (i+1)*(b-a)/width) ) {
			int j = round(a + i*(b-a)/width);
			if(j < 0) j = 0;
			if(j >= p->sample_count) j = p->sample_count-1;

			int k = round((0.1+p->samples_sc[j]/max)*0.8*height);

			if(first) {
				cairo_move_to(c,i+.5,height-k-.5);
				first = 0;
			} else
				cairo_line_to(c,i+.5,height-k-.5);
		}
	}
}

double amplitude_to_time(double lift_angle, double amp)
{
	return asin(lift_angle / (2 * amp)) / M_PI;
}

int guess_bph(double period)
{
	double bph = 7200 / period;
	double min = bph;
	int i,ret;

	ret = 0;
	for(i=0; preset_bph[i]; i++) {
		double diff = fabs(bph - preset_bph[i]);
		if(diff < min) {
			min = diff;
			ret = i;
		}
	}

	return preset_bph[ret];
}

double draw_watch_icon(cairo_t *c, int signal)
{
	int happy = !!signal;
	cairo_set_line_width(c,3);
	cairo_set_source(c,happy?green:red);
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.75, OUTPUT_WINDOW_HEIGHT * (0.75 - 0.5*happy));
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.35, OUTPUT_WINDOW_HEIGHT * (0.65 - 0.3*happy));
	cairo_stroke(c);
	cairo_arc(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.4, 0, 2*M_PI);
	cairo_stroke(c);
	const int l = OUTPUT_WINDOW_HEIGHT * 0.8 / (2*NSTEPS - 1);
	int i;
	cairo_set_line_width(c,1);
	for(i = 0; i < signal; i++) {
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 1.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 1.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - (2*i+1)*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - (2*i+1)*l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT + 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.9 - 2*i*l);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	}
	return OUTPUT_WINDOW_HEIGHT + 3*l;
}

gboolean output_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	c = gdk_cairo_create(widget->window);
	cairo_set_font_size(c,OUTPUT_FONT);

	cairo_set_source(c,black);
	cairo_paint(c);

	int old;
	struct processing_buffers *p = w->get_data(w,&old);

	double x = draw_watch_icon(c,w->signal);

	char rates[100];
	char bphs[100];

	if(p) {
		int bph = w->guessed_bph;
		double rate = (7200/(bph*p->period / p->sample_rate) - 1)*24*3600;
		double be = fabs(p->be) * 1000 / p->sample_rate;
		rate = round(rate);
		if(rate == 0) rate = 0;
		sprintf(rates,"%s%.0f s/d   %.1f ms   ",rate > 0 ? "+" : "",rate,be);
		sprintf(bphs,"%d bph",bph);
	} else {
		strcpy(rates,"--- s/d   --- ms    ");
		sprintf(bphs,"%d bph",w->guessed_bph);
	}

	if(p && old)
		cairo_set_source(c,yellow);
	else
		cairo_set_source(c,white);

	cairo_text_extents_t extents;

	cairo_text_extents(c,"0",&extents);
	double y = (double)OUTPUT_WINDOW_HEIGHT/2 - extents.y_bearing - extents.height/2;

	cairo_move_to(c,x,y);
	cairo_show_text(c,rates);
	cairo_text_extents(c,rates,&extents);
	x += extents.x_advance;

	cairo_set_source(c,white);
	cairo_move_to(c,x,y);
	cairo_show_text(c,bphs);
	cairo_text_extents(c,bphs,&extents);
	x += extents.x_advance;

	cairo_destroy(c);

	return FALSE;
}

struct interactive_w_data {
	struct processing_buffers *bfs;
	struct processing_buffers *old;
};

gboolean amp_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	int width = w->amp_drawing_area->allocation.width;
	int height = w->amp_drawing_area->allocation.height;
	int font = width / 90;
	if(font < 12)
		font = 12;
	int i;
	double span = amplitude_to_time(w->la,120);

	c = gdk_cairo_create(widget->window);
	cairo_set_line_width(c,1);
	cairo_set_font_size(c,font);

	cairo_set_source(c,black);
	cairo_paint(c);

	int old = 0;
	//struct processing_buffers *p = w->get_data(w,&old);
	struct processing_buffers *p = ((struct interactive_w_data *)w->data)->bfs;

	if(p) {
		double span_time = p->period * span;

		double a = p->period / 10; /// 2 - span_time;
		double b = p->period * 2; // / 2 + span_time;

		draw_amp_graph(a,b,c,p,w->amp_drawing_area);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke(c);
	}

	cairo_destroy(c);

	return FALSE;
}

void expose_waveform(cairo_t *c, struct main_window *w, GtkWidget *da, int (*get_offset)(struct processing_buffers*))
{
	int width = da->allocation.width;
	int height = da->allocation.height;
	int font = width / 90;
	if(font < 12)
		font = 12;
	int i;

	cairo_set_line_width(c,1);
	cairo_set_font_size(c,font);

	cairo_set_source(c,black);
	cairo_paint(c);

	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
		cairo_move_to(c, x + .5, height / 2 + .5);
		cairo_line_to(c, x + .5, height - .5);
		if(i%5)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}
	cairo_set_source(c,white);
	for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
		int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
		if(!(i%5)) {
			char s[10];
			sprintf(s,"%d",i);
			cairo_move_to(c,x+font/4,height-font/2);
			cairo_show_text(c,s);
		}
	}

	int old;
	struct processing_buffers *p = w->get_data(w,&old);
	double period = p ? p->period / p->sample_rate : 7200. / w->guessed_bph;

	for(i = 10; i < 360; i+=10) {
		if(2*i < w->la) continue;
		double t = period*amplitude_to_time(w->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height / 2 + .5);
		if(i % 50)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}
	cairo_set_source(c,white);
	for(i = 50; i < 360; i+=50) {
		double t = period*amplitude_to_time(w->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		char s[10];
		sprintf(s,"%d",abs(i));
		cairo_move_to(c,x+font/4,font * 3 / 2);
		cairo_show_text(c,s);
	}

	if(p) {
		double span = 0.001 * p->sample_rate;
		int offset = get_offset(p);

		double a = offset - span * NEGATIVE_SPAN;
		double b = offset + span * POSITIVE_SPAN;

		draw_graph(a,b,c,p,da);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		cairo_set_source(c,yellow);
		cairo_stroke(c);
	}

//	cairo_set_source(c,white);
//	cairo_move_to(c,font/2,3*font/2);
//	cairo_show_text(c,"Beat error (ms)");

	cairo_destroy(c);
}

int get_tic(struct processing_buffers *p)
{
	return p->tic;
}

int get_toc(struct processing_buffers *p)
{
	return p->toc;
}

gboolean tic_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	expose_waveform(gdk_cairo_create(widget->window), w, w->tic_drawing_area, get_tic);
	return FALSE;
}

gboolean toc_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	expose_waveform(gdk_cairo_create(widget->window), w, w->toc_drawing_area, get_toc);
	return FALSE;
}

gboolean waveform_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	cairo_t *c;

	int width = w->waveform_drawing_area->allocation.width;
	int height = w->waveform_drawing_area->allocation.height;
	int font = width / 90;
	if(font < 12)
		font = 12;

	c = gdk_cairo_create(widget->window);
	cairo_set_line_width(c,1);
	cairo_set_font_size(c,font);

	cairo_set_source(c,black);
	cairo_paint(c);

	int old;
	struct processing_buffers *p = w->get_data(w,&old);

	if(p) {
		int i;
		float max = 0;
		int max_i = 0;

		for(i=0; i<p->period; i++)
			if(p->waveform[i] > max) {
				max = p->waveform[i];
				max_i = i;
			}

		int first = 1;
		for(i=0; i<width; i++) {
			if( round(i*p->period/width) != round((i+1)*p->period/width) ) {
				int j = round(i*p->period/width);
				//j = fmod(j + max_i, p->period);
				if(j < 0) j = 0;
				if(j >= p->sample_count) j = p->sample_count-1;

				int k = round((p->waveform[j]+max/10)*(height-1)/(max*1.1));
				if(k < 0) k = 0;
				if(k >= height) k = height-1;

				if(first) {
					cairo_move_to(c,i+.5,height-k-.5);
					first = 0;
				} else
					cairo_line_to(c,i+.5,height-k-.5);
			}
		}
		cairo_set_source(c,old?yellow:white);
		cairo_stroke(c);
	}

	cairo_destroy(c);

	return FALSE;
}

extern volatile uint64_t timestamp;

gboolean paperstrip_expose_event(GtkWidget *widget, GdkEvent *event, struct main_window *w)
{
	int i,old;
	struct processing_buffers *p = w->get_data(w,&old);

	if(p && !old) {
		uint64_t last = w->events[w->events_wp];
		for(i=0; i<EVENTS_MAX && p->events[i]; i++)
			if(p->events[i] > last + floor(p->period / 4)) {
				if(++w->events_wp == EVENTS_COUNT) w->events_wp = 0;
				w->events[w->events_wp] = p->events[i];
				debug("event at %llu\n",w->events[w->events_wp]);
			}
		w->events_from = p->timestamp - ceil(p->period);
	} else {
		w->events_from = timestamp;
	}

	cairo_t *c;

	int width = w->paperstrip_drawing_area->allocation.width;
	int height = w->paperstrip_drawing_area->allocation.height;

	c = gdk_cairo_create(widget->window);

	cairo_set_source(c,black);
	cairo_paint(c);

	cairo_set_source(c,white);

	int strip_width = width * 9 / 10;
	double sweep = PA_SAMPLE_RATE * 3600. / w->guessed_bph;
	double now = sweep*ceil(timestamp/sweep);
	for(i = w->events_wp;;) {
		if(!w->events[i]) break;
		int column = floor(fmod(now - w->events[i], (sweep / PAPERSTRIP_ZOOM)) * strip_width / (sweep / PAPERSTRIP_ZOOM));
		int row = floor((now - w->events[i]) / sweep);
		if(row >= height) break;
		cairo_move_to(c,column,row);
		cairo_line_to(c,column+1,row);
		cairo_line_to(c,column+1,row+1);
		cairo_line_to(c,column,row+1);
		cairo_line_to(c,column,row);
		cairo_fill(c);
		if(column < width - strip_width && row > 0) {
			column += strip_width;
			row -= 1;
			cairo_move_to(c,column,row);
			cairo_line_to(c,column+1,row);
			cairo_line_to(c,column+1,row+1);
			cairo_line_to(c,column,row+1);
			cairo_line_to(c,column,row);
			cairo_fill(c);
		}
		if(--i < 0) i = EVENTS_COUNT - 1;
		if(i == w->events_wp) break;
	}

	cairo_destroy(c);

	return FALSE;
}

void redraw(struct main_window *w)
{
	gtk_widget_queue_draw_area(w->output_drawing_area,0,0,w->output_drawing_area->allocation.width,w->output_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->amp_drawing_area,0,0,w->amp_drawing_area->allocation.width,w->amp_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->tic_drawing_area,0,0,w->tic_drawing_area->allocation.width,w->tic_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->toc_drawing_area,0,0,w->toc_drawing_area->allocation.width,w->toc_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->waveform_drawing_area,0,0,w->waveform_drawing_area->allocation.width,w->waveform_drawing_area->allocation.height);
	gtk_widget_queue_draw_area(w->paperstrip_drawing_area,0,0,w->paperstrip_drawing_area->allocation.width,w->paperstrip_drawing_area->allocation.height);
}

void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
	char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));
	if(s) {
		int n;
		char *t;
		n = strtol(s,&t,10);
		if(*t || n < MIN_BPH || n > MAX_BPH) w->bph = 0;
		else w->bph = w->guessed_bph = n;
		g_free(s);
		w->recompute(w);
		redraw(w);
	}
}

void handle_la_change(GtkSpinButton *b, struct main_window *w)
{
	double la = gtk_spin_button_get_value(b);
	if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
	w->la = la;
	redraw(w);
}

void init_main_window(struct main_window *w)
{
	w->signal = 0;

	w->events = malloc(EVENTS_COUNT * sizeof(uint64_t));
	memset(w->events,0,EVENTS_COUNT * sizeof(uint64_t));
	w->events_wp = 0;
	w->events_from = 0;

	w->guessed_bph = w->last_bph = DEFAULT_BPH;
	w->bph = 0;
	w->la = DEFAULT_LA;

	w->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(w->window),10);
	g_signal_connect(G_OBJECT(w->window),"delete_event",G_CALLBACK(delete_event),NULL);
	g_signal_connect(G_OBJECT(w->window),"destroy",G_CALLBACK(w->destroy),w);

	GtkWidget *vbox = gtk_vbox_new(FALSE,10);
	gtk_container_add(GTK_CONTAINER(w->window),vbox);
	gtk_widget_show(vbox);

	GtkWidget *hbox = gtk_hbox_new(FALSE,10);
	gtk_box_pack_start(GTK_BOX(vbox),hbox,FALSE,TRUE,0);
	gtk_widget_show(hbox);

	GtkWidget *label = gtk_label_new("bph");
	GTK_WIDGET_SET_FLAGS(label,GTK_NO_WINDOW);
	gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
	gtk_widget_show(label);

	w->bph_combo_box = gtk_combo_box_text_new_with_entry();
	gtk_box_pack_start(GTK_BOX(hbox),w->bph_combo_box,FALSE,TRUE,0);
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box),"guess");
	int *bph;
	for(bph = preset_bph; *bph; bph++) {
		char s[100];
		sprintf(s,"%d",*bph);
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box),s);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box),0);
	gtk_signal_connect(GTK_OBJECT(w->bph_combo_box),"changed",(GtkSignalFunc)handle_bph_change,w);
	gtk_widget_show(w->bph_combo_box);

	label = gtk_label_new("lift angle");
	GTK_WIDGET_SET_FLAGS(label,GTK_NO_WINDOW);
	gtk_box_pack_start(GTK_BOX(hbox),label,FALSE,FALSE,0);
	gtk_widget_show(label);

	w->la_spin_button = gtk_spin_button_new_with_range(MIN_LA,MAX_LA,1);
	gtk_box_pack_start(GTK_BOX(hbox),w->la_spin_button,FALSE,TRUE,0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button),DEFAULT_LA);
	gtk_signal_connect(GTK_OBJECT(w->la_spin_button),"value_changed",(GtkSignalFunc)handle_la_change,w);
	gtk_widget_show(w->la_spin_button);

	GtkWidget *hbox2 = gtk_hbox_new(FALSE,10);
	gtk_box_pack_start(GTK_BOX(vbox),hbox2,TRUE,TRUE,0);
	gtk_widget_show(hbox2);

	w->paperstrip_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->paperstrip_drawing_area),200,500);
	gtk_box_pack_start(GTK_BOX(hbox2),w->paperstrip_drawing_area,FALSE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->paperstrip_drawing_area),"expose_event",
			(GtkSignalFunc)paperstrip_expose_event, w);
	gtk_widget_set_events(w->paperstrip_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->paperstrip_drawing_area);

	GtkWidget *vbox2 = gtk_vbox_new(FALSE,10);
	gtk_box_pack_start(GTK_BOX(hbox2),vbox2,TRUE,TRUE,0);
	gtk_widget_show(vbox2);

	w->output_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->output_drawing_area),500,OUTPUT_WINDOW_HEIGHT);
	gtk_box_pack_start(GTK_BOX(vbox2),w->output_drawing_area,FALSE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->output_drawing_area),"expose_event",
			(GtkSignalFunc)output_expose_event, w);
	gtk_widget_set_events(w->output_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->output_drawing_area);

	w->tic_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->tic_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox2),w->tic_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->tic_drawing_area),"expose_event",
			(GtkSignalFunc)tic_expose_event, w);
	gtk_widget_set_events(w->tic_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->tic_drawing_area);

	w->toc_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->toc_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox2),w->toc_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->toc_drawing_area),"expose_event",
			(GtkSignalFunc)toc_expose_event, w);
	gtk_widget_set_events(w->toc_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->toc_drawing_area);

	w->amp_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->amp_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox2),w->amp_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->amp_drawing_area),"expose_event",
			(GtkSignalFunc)amp_expose_event, w);
	gtk_widget_set_events(w->amp_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->amp_drawing_area);

	w->waveform_drawing_area = gtk_drawing_area_new();
	gtk_drawing_area_size(GTK_DRAWING_AREA(w->waveform_drawing_area),500,200);
	gtk_box_pack_start(GTK_BOX(vbox2),w->waveform_drawing_area,TRUE,TRUE,0);
	gtk_signal_connect(GTK_OBJECT(w->waveform_drawing_area),"expose_event",
			(GtkSignalFunc)waveform_expose_event, w);
	gtk_widget_set_events(w->waveform_drawing_area, GDK_EXPOSURE_MASK);
	gtk_widget_show(w->waveform_drawing_area);

	gtk_window_maximize(GTK_WINDOW(w->window));
	gtk_widget_show(w->window);
}

void quit()
{
	gtk_main_quit();
}

struct processing_buffers *int_get_data(struct main_window *w, int *old)
{
	struct interactive_w_data *d = w->data;
	struct processing_buffers *p = d->bfs;
	int i;
	for(i=0; i<NSTEPS && p[i].ready; i++);
	if(i && p[i-1].sigma < p[i-1].period / 10000) {
		if(d->old) pb_destroy(d->old);
		d->old = pb_clone(&p[i-1]);
		*old = 0;
		return &p[i-1];
	} else {
		*old = 1;
		return d->old;
	}
}

void int_recompute(struct main_window *w)
{
	struct processing_buffers *p = ((struct interactive_w_data *)w->data)->bfs;
	w->signal = analyze_pa_data(p, w->bph, w->events_from);
	int old;
	p = int_get_data(w,&old);
	if(old) w->signal = -w->signal;
	if(p)
		w->guessed_bph = w->bph ? w->bph : guess_bph(p->period / p->sample_rate);
}

guint refresh(struct main_window *w)
{
	w->recompute(w);
	redraw(w);
	return TRUE;
}

int run_interactively()
{
	struct processing_buffers p[NSTEPS];
	int i;
	for(i=0; i<NSTEPS; i++) {
		p[i].sample_rate = PA_SAMPLE_RATE;
		p[i].sample_count = PA_SAMPLE_RATE * (1<<(i+FIRST_STEP));
		setup_buffers(&p[i]);
		p[i].period = -1;
	}
	if(start_portaudio()) return 1;

	struct main_window w;
	struct interactive_w_data d;
	d.bfs = p;
	d.old = NULL;
	w.destroy = quit;
	w.data = &d;
	w.get_data = int_get_data;
	w.recompute = int_recompute;

	init_main_window(&w);

	g_timeout_add_full(G_PRIORITY_LOW,100,(GSourceFunc)refresh,&w,NULL);

	gtk_main();

	return 0;
}

int main(int argc, char **argv)
{
	gtk_init(&argc, &argv);
	initialize_palette();

	return run_interactively();
}
