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

cairo_pattern_t *black,*white,*red,*green,*blue,*blueish,*yellow;

static void define_color(cairo_pattern_t **gc,double r,double g,double b)
{
	*gc = cairo_pattern_create_rgb(r,g,b);
}

void initialize_palette()
{
	define_color(&black,0,0,0);
	define_color(&white,1,1,1);
	define_color(&red,1,0,0);
	define_color(&green,0,0.8,0);
	define_color(&blue,0,0,1);
	define_color(&blueish,0,0,.5);
	define_color(&yellow,1,1,0);
}

static void draw_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	GtkAllocation temp;
	gtk_widget_get_allocation (da, &temp);
	int width = temp.width;
	int height = temp.height;

	int n;

	int first = 1;
	for(n=0; n<2*width; n++) {
		int i = n < width ? n : 2*width - 1 - n;
		double x = fmod(a + i * (b-a) / width, p->period);
		if(x < 0) x += p->period;
		int j = floor(x);
		double y;

		if(p->waveform[j] <= 0) y = 0;
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

#ifdef DEBUG
static void draw_debug_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
	if(!p->debug) return;

	GtkAllocation temp;
	gtk_widget_get_allocation (da, &temp);
	int width = temp.width;
	int height = temp.height;

	int i;
	float max = 0;

	int ai = round(a);
	int bi = 1+round(b);
	if(ai < 0) ai = 0;
	if(bi > p->sample_count) bi = p->sample_count;
	for(i=ai; i<bi; i++)
		if(p->debug[i] > max)
			max = p->debug[i];

	int first = 1;
	for(i=0; i<width; i++) {
		if( round(a + i*(b-a)/width) != round(a + (i+1)*(b-a)/width) ) {
			int j = round(a + i*(b-a)/width);
			if(j < 0) j = 0;
			if(j >= p->sample_count) j = p->sample_count-1;

			int k = round((0.1+p->debug[j]/max)*0.8*height);

			if(first) {
				cairo_move_to(c,i+.5,height-k-.5);
				first = 0;
			} else
				cairo_line_to(c,i+.5,height-k-.5);
		}
	}
}
#endif

static double amplitude_to_time(double lift_angle, double amp)
{
	return asin(lift_angle / (2 * amp)) / M_PI;
}

static double draw_watch_icon(cairo_t *c, int signal, int happy, int light)
{
	happy = !!happy;
	cairo_set_line_width(c,3);
	cairo_set_source(c,happy?green:red);
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.75, OUTPUT_WINDOW_HEIGHT * (0.75 - 0.5*happy));
	cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5);
	cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.35, OUTPUT_WINDOW_HEIGHT * (0.65 - 0.3*happy));
	cairo_stroke(c);
	cairo_arc(c, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.5, OUTPUT_WINDOW_HEIGHT * 0.4, 0, 2*M_PI);
	cairo_stroke(c);
	int l = OUTPUT_WINDOW_HEIGHT * 0.8 / (2*NSTEPS - 1);
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
	if(light) {
		int l = OUTPUT_WINDOW_HEIGHT * 0.15;
		cairo_set_line_width(c,2);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.2);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.5*l, OUTPUT_WINDOW_HEIGHT * 0.2 + l);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5        , OUTPUT_WINDOW_HEIGHT * 0.2 + l);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 - 0.2*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.8*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_move_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.3*l, OUTPUT_WINDOW_HEIGHT * 0.2 + 1);
		cairo_line_to(c, OUTPUT_WINDOW_HEIGHT * 0.5 + 0.3*l, OUTPUT_WINDOW_HEIGHT * 0.2 + l + 1);
		cairo_stroke(c);
	}
	return OUTPUT_WINDOW_HEIGHT + 3*l;
}

static void cairo_init(cairo_t *c)
{
	cairo_set_line_width(c,1);

	cairo_set_source(c,black);
	cairo_paint(c);
}

static double print_s(cairo_t *c, double x, double y, char *s)
{
	cairo_text_extents_t extents;
	cairo_move_to(c,x,y);
	cairo_show_text(c,s);
	cairo_text_extents(c,s,&extents);
	x += extents.x_advance;
	return x;
}

static double print_number(cairo_t *c, double x, double y, char *s)
{
	cairo_text_extents_t extents;
	cairo_text_extents(c,"0",&extents);
	double z = extents.x_advance;
	char t[2];
	t[1] = 0;
	while((t[0] = *s++)) {
		cairo_text_extents(c,t,&extents);
		cairo_move_to(c, x + (z - extents.x_advance) / 2, y);
		cairo_show_text(c,t);
		x += z;
	}
	return x;
}

static gboolean output_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	cairo_init(c);

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int old = snst->is_old;

	double x = draw_watch_icon(c,snst->signal,snst->calibrate ? snst->signal==NSTEPS : snst->signal, snst->is_light);

	cairo_text_extents_t extents;

	cairo_set_font_size(c, OUTPUT_FONT);
	cairo_text_extents(c,"0",&extents);
	double y = (double)OUTPUT_WINDOW_HEIGHT/2 - extents.y_bearing - extents.height/2;

	if(snst->calibrate) {
		cairo_set_source(c, white);
		x = print_s(c,x,y,"cal");
		cairo_set_font_size(c, OUTPUT_FONT*2/3);
		x = print_s(c,x,y," (");
		cairo_move_to(c,x,y);
		{
			double a = 0;
			char *s[] = {"wait", "acq.", "done", "fail", NULL}, **t = s;
			for(;*t;t++) {
				cairo_text_extents(c,*t,&extents);
				if(a < extents.x_advance) a = extents.x_advance;
			}
			x += a;
		}
		switch(snst->cal_state) {
			case 1:
				cairo_set_source(c,green);
				cairo_show_text(c,"done");
				break;
			case 0:
				cairo_set_source(c, snst->signal == NSTEPS ? white : yellow);
				cairo_show_text(c, snst->signal == NSTEPS ? "acq." : "wait");
				break;
			case -1:
				cairo_set_source(c,red);
				cairo_show_text(c,"fail");
				break;
		}
		cairo_set_source(c, white);
		x = print_s(c,x,y,")");
		cairo_set_font_size(c, OUTPUT_FONT);
		char s[20];
		switch(snst->cal_state) {
			case 1:
				sprintf(s, " %s%d.%d",
						snst->cal_result < 0 ? "-" : "+",
						abs(snst->cal_result) / 10,
						abs(snst->cal_result) % 10 );
				x = print_s(c,x,y,s);
				cairo_set_font_size(c, OUTPUT_FONT*2/3);
				x = print_s(c,x,y," s/d");
				break;
			case 0:
				sprintf(s, " %d", snst->cal_percent);
				x = print_number(c,x,y,s);
				x = print_s(c,x,y," %");
				break;
		}
	} else {
		char outputs[8][20];
		if(p) {
			int rate = round(snst->rate);
			double be = snst->be;
			char rates[20];
			sprintf(rates,"%s%d",rate > 0 ? "+" : rate < 0 ? "-" : "",abs(rate));
			sprintf(outputs[0],"%4s",rates);
			sprintf(outputs[2]," %4.1f",be);
			if(snst->amp > 0)
				sprintf(outputs[4]," %3.0f",snst->amp);
			else
				strcpy(outputs[4]," ---");
		} else {
			strcpy(outputs[0],"----");
			strcpy(outputs[2]," ----");
			strcpy(outputs[4]," ---");
		}
		sprintf(outputs[6]," %d",snst->guessed_bph);

		strcpy(outputs[1]," s/d");
		strcpy(outputs[3]," ms");
		strcpy(outputs[5]," deg");
		strcpy(outputs[7]," bph");

		int i;
		for(i=0; i<8; i++) {
			if(i%2) {
				cairo_set_source(c, white);
				cairo_set_font_size(c, OUTPUT_FONT*2/3);
				x = print_s(c,x,y,outputs[i]);
			} else {
				cairo_set_source(c, i > 4 || !p || !old ? white : yellow);
				cairo_set_font_size(c, OUTPUT_FONT);
				x = print_number(c,x,y,outputs[i]);
			}
		}
	}
#ifdef DEBUG
	{
		static GTimer *timer = NULL;
		if (!timer) timer = g_timer_new();
		else {
			char s[100];
			sprintf(s,"  %.2f fps",1./g_timer_elapsed(timer, NULL));
			cairo_set_source(c, white);
			cairo_set_font_size(c, OUTPUT_FONT);
			x = print_s(c,x,y,s);
			g_timer_reset(timer);
		}
	}
#endif

	return FALSE;
}

static void expose_waveform(
			struct output_panel *op,
			GtkWidget *da,
			cairo_t *c,
			int (*get_offset)(struct processing_buffers*),
			double (*get_pulse)(struct processing_buffers*))
{
	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation(da, &temp);

	const int width = temp.width, height = temp.height;

	int fontw = width / 40;
	fontw = fontw < 12 ? 12 : fontw > 20 ? 20 : fontw;
	int fonth = height / 12;
	fonth = fonth < 12 ? 12 : fonth > 20 ? 20 : fonth;
	const int font = MIN(fontw, fonth);
	cairo_set_font_size(c, font);

	cairo_font_extents_t fextents;
	cairo_font_extents(c, &fextents);

	const int margin = 6;

	int i;
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
		if(!(i%5)) {
			int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
			char s[10];
			sprintf(s,"%d",i);
			cairo_move_to(c,x+font/4, height - margin);
			cairo_show_text(c,s);
		}
	}

	cairo_text_extents_t extents;

	cairo_text_extents(c,"ms",&extents);
	cairo_move_to(c,width - extents.x_advance - font/4, height - margin);
	cairo_show_text(c,"ms");

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int old = snst->is_old;
	double period = p ? p->period / snst->sample_rate : 7200. / snst->guessed_bph;

	for(i = 10; i < 360; i+=10) {
		if(2*i < snst->la) continue;
		double t = period*amplitude_to_time(snst->la,i);
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

	double last_x = 0;
	cairo_set_source(c,white);
	for(i = 50; i < 360; i+=50) {
		double t = period*amplitude_to_time(snst->la,i);
		if(t > .001 * NEGATIVE_SPAN) continue;
		int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
		if(x > last_x) {
			char s[10];

			sprintf(s,"%d",abs(i));
			cairo_move_to(c, x + font/4, margin + fextents.ascent);
			cairo_show_text(c,s);
			cairo_text_extents(c,s,&extents);
			last_x = x + font/4 + extents.x_advance;
		}
	}

	cairo_text_extents(c,"deg",&extents);
	cairo_move_to(c,width - extents.x_advance - font/4, margin + fextents.ascent);
	cairo_show_text(c,"deg");

	if(p) {
		double span = 0.001 * snst->sample_rate;
		int offset = get_offset(p);

		double a = offset - span * NEGATIVE_SPAN;
		double b = offset + span * POSITIVE_SPAN;

		draw_graph(a,b,c,p,da);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke_preserve(c);
		cairo_fill(c);

		double pulse = get_pulse(p);
		if(pulse > 0) {
			int x = round((NEGATIVE_SPAN - pulse / span) * width / (POSITIVE_SPAN + NEGATIVE_SPAN));
			cairo_move_to(c, x, 1);
			cairo_line_to(c, x, height - 1);
			cairo_set_source(c,blue);
			cairo_set_line_width(c,2);
			cairo_stroke(c);
		}
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		cairo_set_source(c,yellow);
		cairo_stroke(c);
	}
}

static int get_tic(struct processing_buffers *p)
{
	return p->tic;
}

static int get_toc(struct processing_buffers *p)
{
	return p->toc;
}

static double get_tic_pulse(struct processing_buffers *p)
{
	return p->tic_pulse;
}

static double get_toc_pulse(struct processing_buffers *p)
{
	return p->toc_pulse;
}

static gboolean tic_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	expose_waveform(op, op->tic_drawing_area, c, get_tic, get_tic_pulse);
	return FALSE;
}

static gboolean toc_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	expose_waveform(op, op->toc_drawing_area, c, get_toc, get_toc_pulse);
	return FALSE;
}

static gboolean period_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation (op->period_drawing_area, &temp);

	int width = temp.width;
	int height = temp.height;

	struct snapshot *snst = op->snst;
	struct processing_buffers *p = snst->pb;
	int old = snst->is_old;

	double toc,a=0,b=0;

	if(p) {
		toc = p->tic < p->toc ? p->toc : p->toc + p->period;
		a = ((double)p->tic + toc)/2 - p->period/2;
		b = ((double)p->tic + toc)/2 + p->period/2;

		cairo_move_to(c, (p->tic - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
		cairo_line_to(c, (p->tic - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
		cairo_line_to(c, (p->tic - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
		cairo_line_to(c, (p->tic - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
		cairo_set_source(c,blueish);
		cairo_fill(c);

		cairo_move_to(c, (toc - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
		cairo_line_to(c, (toc - a - NEGATIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
		cairo_line_to(c, (toc - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, height);
		cairo_line_to(c, (toc - a + POSITIVE_SPAN*.001*snst->sample_rate) * width/p->period, 0);
		cairo_set_source(c,blueish);
		cairo_fill(c);
	}

	int i;
	for(i = 1; i < 16; i++) {
		int x = i * width / 16;
		cairo_move_to(c, x+.5, .5);
		cairo_line_to(c, x+.5, height - .5);
		if(i % 4)
			cairo_set_source(c,green);
		else
			cairo_set_source(c,red);
		cairo_stroke(c);
	}

	if(p) {
		draw_graph(a,b,c,p,op->period_drawing_area);

		cairo_set_source(c,old?yellow:white);
		cairo_stroke_preserve(c);
		cairo_fill(c);
	} else {
		cairo_move_to(c, .5, height / 2 + .5);
		cairo_line_to(c, width - .5, height / 2 + .5);
		cairo_set_source(c,yellow);
		cairo_stroke(c);
	}

	return FALSE;
}

static gboolean paperstrip_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	int i;
	const struct snapshot *snst = op->snst;
	struct display *ssd = snst->d;
	uint64_t time = snst->timestamp ? snst->timestamp : get_timestamp(snst->is_light);
	double sweep;
	int zoom_factor;
	double slope = 1000; // detected rate: 1000 -> do not display

	// Allocate initial display parameters.  Will persist across each new
	// snapshot after this.
	if (!ssd) {
		ssd = op->snst->d = calloc(1, sizeof(*snst->d));
	}

	if(snst->calibrate) {
		sweep = snst->nominal_sr;
		zoom_factor = PAPERSTRIP_ZOOM_CAL;
		slope = (double) snst->cal * zoom_factor / (10 * 3600 * 24);
	} else {
		sweep = snst->sample_rate * 3600. / snst->guessed_bph;
		zoom_factor = PAPERSTRIP_ZOOM;
		if(snst->events_count && snst->events[snst->events_wp])
			slope = - snst->rate * zoom_factor / (3600. * 24.);
	}

	cairo_init(c);

	GtkAllocation temp;
	gtk_widget_get_allocation(widget, &temp);
	int width, height;

	/* The paperstrip is coded to be vertical; horizontal uses cairo to rotate it. */
	if(op->vertical_layout) {
		width = temp.width;
		height = temp.height;
	} else {
		width = temp.height;
		height = temp.width;

		cairo_translate(c, height, 0);
		cairo_rotate(c, M_PI/2);
	}

	int stopped = 0;
	if( snst->events_count &&
	    snst->events[snst->events_wp] &&
	    time > 5 * snst->nominal_sr + snst->events[snst->events_wp]) {
		time = 5 * snst->nominal_sr + snst->events[snst->events_wp];
		stopped = 1;
	}

	int strip_width = round(width / (1 + PAPERSTRIP_MARGIN));

	cairo_set_line_width(c,1.3);

	slope *= strip_width;
	if(slope <= 2 && slope >= -2) {
		for(i=0; i<4; i++) {
			double y = 0;
			cairo_move_to(c, (double)width * (i+.5) / 4, 0);
			for(;;) {
				double x = y * slope + (double)width * (i+.5) / 4;
				x = fmod(x, width);
				if(x < 0) x += width;
				double nx = x + slope * (height - y);
				if(nx >= 0 && nx <= width) {
					cairo_line_to(c, nx, height);
					break;
				} else {
					double d = slope > 0 ? width - x : x;
					y += d / fabs(slope);
					cairo_line_to(c, slope > 0 ? width : 0, y);
					y += 1;
					if(y > height) break;
					cairo_move_to(c, slope > 0 ? 0 : width, y);
				}
			}
		}
		cairo_set_source(c, blue);
		cairo_stroke(c);
	}

	cairo_set_line_width(c,1);

	int left_margin = (width - strip_width) / 2;
	int right_margin = (width + strip_width) / 2;
	cairo_move_to(c, left_margin + .5, .5);
	cairo_line_to(c, left_margin + .5, height - .5);
	cairo_move_to(c, right_margin + .5, .5);
	cairo_line_to(c, right_margin + .5, height - .5);
	cairo_set_source(c, green);
	cairo_stroke(c);

	double now = sweep*ceil(time/sweep);
	double ten_s = snst->sample_rate * 10 / sweep;
	double last_line = fmod(now/sweep, ten_s);
	int last_tenth = floor(now/(sweep*ten_s));
	for(i=0;;i++) {
		double y = 0.5 + round(last_line + i*ten_s);
		if(y > height) break;
		cairo_move_to(c, .5, y);
		cairo_line_to(c, width-.5, y);
		cairo_set_source(c, (last_tenth-i)%6 ? green : red);
		cairo_stroke(c);
	}

	cairo_set_source(c,stopped?yellow:white);
	for(i = snst->events_wp;;) {
		if(!snst->events_count || !snst->events[i]) break;
		double event = now - snst->events[i] + snst->d->trace_centering + sweep * PAPERSTRIP_MARGIN / (2 * zoom_factor);
		int column = floor(fmod(event, (sweep / zoom_factor)) * strip_width / (sweep / zoom_factor));
		int row = floor(event / sweep);
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
		if(--i < 0) i = snst->events_count - 1;
		if(i == snst->events_wp) break;
	}

	cairo_set_source(c,white);
	cairo_set_line_width(c,2);
	cairo_move_to(c, left_margin + 3, height - 20.5);
	cairo_line_to(c, right_margin - 3, height - 20.5);
	cairo_stroke(c);
	cairo_set_line_width(c,1);
	cairo_move_to(c, left_margin + .5, height - 20.5);
	cairo_line_to(c, left_margin + 5.5, height - 15.5);
	cairo_line_to(c, left_margin + 5.5, height - 25.5);
	cairo_line_to(c, left_margin + .5, height - 20.5);
	cairo_fill(c);
	cairo_move_to(c, right_margin + .5, height - 20.5);
	cairo_line_to(c, right_margin - 4.5, height - 15.5);
	cairo_line_to(c, right_margin - 4.5, height - 25.5);
	cairo_line_to(c, right_margin + .5, height - 20.5);
	cairo_fill(c);

	int font = width / 25;
	cairo_set_font_size(c, font < 12 ? 12 : font > 24 ? 24 : font);

	char s[32];
	snprintf(s, sizeof(s), "%.1f ms", snst->calibrate ?
				1000. / zoom_factor :
				3600000. / (snst->guessed_bph * zoom_factor));
	cairo_text_extents_t extents;
	cairo_font_extents_t fextents;
	cairo_text_extents(c, s, &extents);
	cairo_font_extents(c, &fextents);
	cairo_move_to(c, (width - extents.width)/2 - extents.x_bearing, (height - 25.5) + fextents.ascent - fextents.height);
	cairo_show_text(c,s);

	return FALSE;
}

#ifdef DEBUG
static gboolean debug_draw_event(GtkWidget *widget, cairo_t *c, struct output_panel *op)
{
	UNUSED(widget);
	cairo_init(c);

	struct snapshot *snst = op->snst;
	struct processing_buffers *p;
	if(snst->calibrate)
		p = &op->computer->pdata->buffers[0];
	else
		p = snst->pb;

	if(p) {
		double a = snst->nominal_sr / 10;
		double b = snst->nominal_sr * 2;

		draw_debug_graph(a,b,c,p,op->debug_drawing_area);

		cairo_set_source(c,snst->is_old?yellow:white);
		cairo_stroke(c);
	}

	return FALSE;
}
#endif

static void handle_clear_trace(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	if(op->computer) {
		lock_computer(op->computer);
		if(!op->snst->calibrate) {
			memset(op->snst->events,0,op->snst->events_count*sizeof(uint64_t));
			op->computer->clear_trace = 1;
		}
		unlock_computer(op->computer);
		gtk_widget_queue_draw(op->paperstrip_drawing_area);
	}
}

static void handle_center_trace(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	struct snapshot *snst = op->snst;
	if(!snst || !snst->events)
		return;
	uint64_t last_ev = snst->events[snst->events_wp];
	double new_centering;
	if(last_ev) {
		double sweep;
		if(snst->calibrate)
			sweep = (double) snst->nominal_sr / PAPERSTRIP_ZOOM_CAL;
		else
			sweep = snst->sample_rate * 3600. / (PAPERSTRIP_ZOOM * snst->guessed_bph);
		new_centering = fmod(last_ev + .5*sweep , sweep);
	} else 
		new_centering = 0;
	snst->d->trace_centering = new_centering;
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void shift_trace(struct output_panel *op, double direction)
{
	struct snapshot *snst = op->snst;
	double sweep;
	if(snst->calibrate)
		sweep = (double) snst->nominal_sr / PAPERSTRIP_ZOOM_CAL;
	else
		sweep = snst->sample_rate * 3600. / (PAPERSTRIP_ZOOM * snst->guessed_bph);
	snst->d->trace_centering = fmod(snst->d->trace_centering + sweep * (1.+.1*direction), sweep);
	gtk_widget_queue_draw(op->paperstrip_drawing_area);
}

static void handle_left(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	shift_trace(op,-1);
}

static void handle_right(GtkButton *b, struct output_panel *op)
{
	UNUSED(b);
	shift_trace(op,1);
}

void op_set_snapshot(struct output_panel *op, struct snapshot *snst)
{
	op->snst = snst;
	gtk_widget_set_sensitive(op->clear_button, !snst->calibrate);
}

void op_set_border(struct output_panel *op, int i)
{
	gtk_container_set_border_width(GTK_CONTAINER(op->panel), i);
}

void op_destroy(struct output_panel *op)
{
	snapshot_destroy(op->snst);
	free(op);
}

/* Creates the paperstrip, with buttons.  Returns top level Widget that contains
 * them.  Vertical controls orientation of paper strip.  */
static GtkWidget* create_paperstrip(struct output_panel *op, bool vertical)
{
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

	// Paperstrip
	op->paperstrip_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->paperstrip_drawing_area, 150, 150);
	gtk_box_pack_start(GTK_BOX(vbox), op->paperstrip_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->paperstrip_drawing_area, "draw", G_CALLBACK(paperstrip_draw_event), op);
	gtk_widget_set_events(op->paperstrip_drawing_area, GDK_EXPOSURE_MASK);

	// Buttons
	GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

	// < button
	op->left_button = gtk_button_new_from_icon_name(
		vertical ? "pan-start-symbolic" : "pan-up-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_box_pack_start(GTK_BOX(hbox), op->left_button, TRUE, TRUE, 0);
	g_signal_connect (op->left_button, "clicked", G_CALLBACK(handle_left), op);

	// CLEAR button
	if(op->computer) {
		op->clear_button = gtk_button_new_with_label("Clear");
		gtk_box_pack_start(GTK_BOX(hbox), op->clear_button, TRUE, TRUE, 0);
		g_signal_connect (op->clear_button, "clicked", G_CALLBACK(handle_clear_trace), op);
		gtk_widget_set_sensitive(op->clear_button, !op->snst->calibrate);
	}

	// CENTER button
	GtkWidget *center_button = gtk_button_new_with_label("Center");
	gtk_box_pack_start(GTK_BOX(hbox), center_button, TRUE, TRUE, 0);
	g_signal_connect (center_button, "clicked", G_CALLBACK(handle_center_trace), op);

	// > button
	op->right_button = gtk_button_new_from_icon_name(
		vertical ? "pan-end-symbolic" : "pan-down-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
	gtk_box_pack_start(GTK_BOX(hbox), op->right_button, TRUE, TRUE, 0);
	g_signal_connect (op->right_button, "clicked", G_CALLBACK(handle_right), op);

	return vbox;
}

/* Create the tic, toc, and period waveforms.  Returns the GtkBox that contains
 * them.  Vertical controls how the waves are stacked.  */
static GtkWidget* create_waveforms(struct output_panel *op, bool vertical)
{
	GtkWidget *box = gtk_box_new(vertical ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, 10);

	// Tic waveform area
	op->tic_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->tic_drawing_area, 300, 150);
	gtk_box_pack_start(GTK_BOX(box), op->tic_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->tic_drawing_area, "draw", G_CALLBACK(tic_draw_event), op);
	gtk_widget_set_events(op->tic_drawing_area, GDK_EXPOSURE_MASK);

	// Toc waveform area
	op->toc_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->toc_drawing_area, 300, 150);
	gtk_box_pack_start(GTK_BOX(box), op->toc_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->toc_drawing_area, "draw", G_CALLBACK(toc_draw_event), op);
	gtk_widget_set_events(op->toc_drawing_area, GDK_EXPOSURE_MASK);

	// Period waveform area
	op->period_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->period_drawing_area, 300, 150);
	gtk_box_pack_start(GTK_BOX(box), op->period_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->period_drawing_area, "draw", G_CALLBACK(period_draw_event), op);
	gtk_widget_set_events(op->period_drawing_area, GDK_EXPOSURE_MASK);

#ifdef DEBUG
	op->debug_drawing_area = gtk_drawing_area_new();
	gtk_box_pack_start(GTK_BOX(box), op->debug_drawing_area, TRUE, TRUE, 0);
	g_signal_connect (op->debug_drawing_area, "draw", G_CALLBACK(debug_draw_event), op);
	gtk_widget_set_events(op->debug_drawing_area, GDK_EXPOSURE_MASK);
#endif

	return box;
}

/* Create container and place paperstrip and waveforms in either vertical or
 * horizontal paperstrip orientation.  Puts container in the panel and shows it. */
static void place_displays(struct output_panel *op, GtkWidget *paperstrip, GtkWidget *waveforms, bool vertical)
{
	op->vertical_layout = vertical;

	op->displays = gtk_paned_new(vertical ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL);
	gtk_paned_set_wide_handle(GTK_PANED(op->displays), TRUE);

	gtk_paned_pack1(GTK_PANED(op->displays), paperstrip, vertical ? FALSE : TRUE, FALSE);

	gtk_orientable_set_orientation(GTK_ORIENTABLE(waveforms), vertical ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL);
	gtk_paned_pack2(GTK_PANED(op->displays), waveforms, TRUE, FALSE);

	/* Make paperstrip arrows buttons point correct way */
	GtkWidget *left_arrow = gtk_button_get_image(GTK_BUTTON(op->left_button));
	gtk_image_set_from_icon_name(GTK_IMAGE(left_arrow),
		vertical ? "pan-start-symbolic" : "pan-up-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);
	GtkWidget *right_arrow = gtk_button_get_image(GTK_BUTTON(op->right_button));
	gtk_image_set_from_icon_name(GTK_IMAGE(right_arrow),
		vertical ? "pan-end-symbolic" : "pan-down-symbolic", GTK_ICON_SIZE_LARGE_TOOLBAR);

	gtk_box_pack_end(GTK_BOX(op->panel), op->displays, TRUE, TRUE, 0);
	gtk_widget_show(op->displays);
}

/* Create the paperstrip and waveforms, a container for them, and place it into
 * the panel.  Returns containing Widget.  Vertical controls paperstrip
 * orientation.  */
static GtkWidget *create_displays(struct output_panel *op, bool vertical)
{
	// The paperstrip and buttons
	op->paperstrip_box = create_paperstrip(op, vertical);
	// Tic/toc/period waveform area
	op->waveforms_box = create_waveforms(op, vertical);

	place_displays(op, op->paperstrip_box, op->waveforms_box, vertical);

	return op->displays;
}

/* Change orientation of existing output panel.  Is a no-op if orientation is
 * not changed.  */
void set_panel_layout(struct output_panel *op, bool vertical)
{
	if (op->vertical_layout == vertical)
		return;

	/* Remove waveforms and paperstrip containers from displays container,
	 * then use place_displays() to put them into a new displays container. 
	 * The need to be refed so they are not deleted when removed from the
	 * container.  */
	g_object_ref(op->waveforms_box);
	gtk_container_remove(GTK_CONTAINER(op->displays), op->waveforms_box);

	g_object_ref(op->paperstrip_box);
	gtk_container_remove(GTK_CONTAINER(op->displays), op->paperstrip_box);

	gtk_widget_destroy(op->displays); op->displays = NULL;
	place_displays(op, op->paperstrip_box, op->waveforms_box, vertical);

	/* They are now refed by op->displays so we don't need our refs anymore */
	g_object_unref(op->paperstrip_box);
	g_object_unref(op->waveforms_box);
}

struct output_panel *init_output_panel(struct computer *comp, struct snapshot *snst, int border, bool vertical)
{
	struct output_panel *op = malloc(sizeof(struct output_panel));

	op->computer = comp;
	op->snst = snst;

	op->panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_set_border_width(GTK_CONTAINER(op->panel), border);

	// Info area on top
	op->output_drawing_area = gtk_drawing_area_new();
	gtk_widget_set_size_request(op->output_drawing_area, 0, OUTPUT_WINDOW_HEIGHT);
	gtk_box_pack_start(GTK_BOX(op->panel),op->output_drawing_area, FALSE, TRUE, 0);
	g_signal_connect (op->output_drawing_area, "draw", G_CALLBACK(output_draw_event), op);
	gtk_widget_set_events(op->output_drawing_area, GDK_EXPOSURE_MASK);

	create_displays(op, vertical);

	return op;
}
