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
#include <stdarg.h>
#include <gtk/gtk.h>

int preset_bph[] = PRESET_BPH;

cairo_pattern_t *black,*white,*red,*green,*blue,*blueish,*yellow;

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
    
    GtkWidget *dialog = gtk_message_dialog_new(NULL,0,GTK_MESSAGE_ERROR,GTK_BUTTONS_CLOSE,"%s",t);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}

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
    define_color(&blue,0,0,1);
    define_color(&blueish,0,0,.5);
    define_color(&yellow,1,1,0);
}

struct main_window {
    GtkWidget *window;
    GtkWidget *bph_combo_box;
    GtkWidget *la_spin_button;
    GtkWidget *info_drawing_area;
    GtkWidget *tic_drawing_area;
    GtkWidget *toc_drawing_area;
    GtkWidget *period_drawing_area;
    GtkWidget *paperstrip_drawing_area;
#ifdef DEBUG
    GtkWidget *debug_drawing_area;
#endif
    
    struct processing_buffers *bfs;
    struct processing_buffers *old;
    
    int bph;
    int guessed_bph;
    int last_bph;
    double la;
    double sample_rate;
    
    uint64_t *events;
    int events_wp;
    uint64_t events_from;
    double trace_centering;
    
    int signal;
};

void redraw(struct main_window *w)
{
    gtk_widget_queue_draw(w->info_drawing_area);
    gtk_widget_queue_draw(w->tic_drawing_area);
    gtk_widget_queue_draw(w->toc_drawing_area);
    gtk_widget_queue_draw(w->period_drawing_area);
    gtk_widget_queue_draw(w->paperstrip_drawing_area);
#ifdef DEBUG
    gtk_widget_queue_draw(w->debug_drawing_area);
#endif
}

/* Find the preset bph value closest corresponding to the current period */
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

struct processing_buffers *get_data(struct main_window *w, int *old)
{
    struct processing_buffers *p = w->bfs;
    int i;
    for(i=0; i<NSTEPS && p[i].ready; i++);
    for(i--; i>=0 && p[i].sigma > p[i].period / 10000; i--);
    if(i>=0) {
        if(w->old) pb_destroy_clone(w->old);
        w->old = pb_clone(&p[i]);
        *old = 0;
        return &p[i];
    } else {
        *old = 1;
        return w->old;
    }
}

void recompute(struct main_window *w)
{
    w->signal = analyze_pa_data(w->bfs, w->bph, w->events_from);
    int old;
    struct processing_buffers *p = get_data(w,&old);
    if(old) w->signal = -w->signal;
    if(p)
        w->guessed_bph = w->bph ? w->bph : guess_bph(p->period / w->sample_rate);
}

/* Called 10 times/second to keep the UI updated */
guint refresh_window(struct main_window *w)
{
    recompute(w);
    redraw(w);
    return TRUE;
}

gboolean delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
    return FALSE;
}

/* Draw the actual waveform */
void draw_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
    int width = gtk_widget_get_allocated_width(da);
    int height = gtk_widget_get_allocated_height(da);
    
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
void draw_debug_graph(double a, double b, cairo_t *c, struct processing_buffers *p, GtkWidget *da)
{
    int width = gtk_widget_get_allocated_width(da);
    int height = gtk_widget_get_allocated_height(da);
    
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

double amplitude_to_time(double lift_angle, double amp)
{
    return asin(lift_angle / (2 * amp)) / M_PI;
}

/* Draw the watch icon at the start of the info area */
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

double get_rate(int bph, double sample_rate, struct processing_buffers *p)
{
    return (7200/(bph*p->period / sample_rate) - 1)*24*3600;
}

double get_amplitude(double la, struct processing_buffers *p)
{
    double ret = -1;
    if(p->tic_pulse > 0 && p->toc_pulse > 0) {
        double tic_amp = la * .5 / sin(M_PI * p->tic_pulse / p->period);
        double toc_amp = la * .5 / sin(M_PI * p->toc_pulse / p->period);
        if(la < tic_amp && tic_amp < 360 && la < toc_amp && toc_amp < 360 && fabs(tic_amp - toc_amp) < 60)
            ret = (tic_amp + toc_amp) / 2;
    }
    return ret;
}

/* Set up default line width and background color before each drawing operation */
void cairo_init(cairo_t *c)
{
    cairo_set_line_width(c, 1);
    
    cairo_set_source(c,black);
    cairo_paint(c);
}

double print_number(cairo_t *c, double x, double y, char *s)
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

/* Draws either the tic or toc waveform & grid in their respective widget */
void draw_waveform(
                   struct main_window *w,
                   GtkWidget *da,
                   cairo_t *cr,
                   int (*get_offset)(struct processing_buffers*),
                   double (*get_pulse)(struct processing_buffers*) )
{
    cairo_init(cr);
    
    int width = gtk_widget_get_allocated_width(da);
    int height = gtk_widget_get_allocated_height(da);
    
    int fontsize = gtk_widget_get_allocated_width(w->window) / 90;
    if(fontsize < 12)
        fontsize = 12;
    int i;

    cairo_set_font_size(cr, fontsize);
    
    for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
        int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
        cairo_move_to(cr, x + .5, height / 2 + .5);
        cairo_line_to(cr, x + .5, height - .5);
        if(i%5)
            cairo_set_source(cr, green);
        else
            cairo_set_source(cr, red);
        cairo_stroke(cr);
    }
    cairo_set_source(cr, white);
    for(i = 1-NEGATIVE_SPAN; i < POSITIVE_SPAN; i++) {
        if(!(i%5)) {
            int x = (NEGATIVE_SPAN + i) * width / (POSITIVE_SPAN + NEGATIVE_SPAN);
            char s[10];
            sprintf(s,"%d",i);
            cairo_move_to(cr,x+fontsize/4, height-fontsize/2);
            cairo_show_text(cr,s);
        }
    }
    
    cairo_text_extents_t extents;
    
    cairo_text_extents(cr,"ms", &extents);
    cairo_move_to(cr,width - extents.x_advance - fontsize/4, height-fontsize/2);
    cairo_show_text(cr, "ms");
    
    int old;
    struct processing_buffers *p = get_data(w, &old);
    double period = p ? p->period / w->sample_rate : 7200. / w->guessed_bph;
    
    for(i = 10; i < 360; i+=10) {
        if(2*i < w->la) continue;
        double t = period*amplitude_to_time(w->la, i);
        if(t > .001 * NEGATIVE_SPAN) continue;
        int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
        cairo_move_to(cr, x+.5, .5);
        cairo_line_to(cr, x+.5, height / 2 + .5);
        if(i % 50)
            cairo_set_source(cr,green);
        else
            cairo_set_source(cr,red);
        cairo_stroke(cr);
    }
    
    double last_x = 0;
    cairo_set_source(cr, white);
    for(i = 50; i < 360; i+=50) {
        double t = period*amplitude_to_time(w->la, i);
        if(t > .001 * NEGATIVE_SPAN) continue;
        int x = round(width * (NEGATIVE_SPAN - 1000*t) / (NEGATIVE_SPAN + POSITIVE_SPAN));
        if(x > last_x) {
            char s[10];
            
            sprintf(s,"%d",abs(i));
            cairo_move_to(cr, x + fontsize/4, fontsize * 3 / 2);
            cairo_show_text(cr, s);
            cairo_text_extents(cr, s, &extents);
            last_x = x + fontsize/4 + extents.x_advance;
        }
    }
    
    cairo_text_extents(cr, "deg", &extents);
    cairo_move_to(cr,width - extents.x_advance - fontsize/4, fontsize * 3 / 2);
    cairo_show_text(cr,"deg");
    
    if(p) {
        double span = 0.001 * w->sample_rate;
        int offset = get_offset(p);
        
        double a = offset - span * NEGATIVE_SPAN;
        double b = offset + span * POSITIVE_SPAN;
        
        draw_graph(a,b,cr,p,da);
        
        cairo_set_source(cr, old?yellow:white);
        cairo_stroke_preserve(cr);
        cairo_fill(cr);
        
        double pulse = get_pulse(p);
        if(pulse > 0) {
            int x = round((NEGATIVE_SPAN - pulse * 1000 / p->sample_rate) * width / (POSITIVE_SPAN + NEGATIVE_SPAN));
            cairo_move_to(cr, x, 1);
            cairo_line_to(cr, x, height - 1);
            cairo_set_source(cr, blue);
            cairo_set_line_width(cr, 2);
            cairo_stroke(cr);
        }
    } else {
        cairo_move_to(cr, .5, height / 2 + .5);
        cairo_line_to(cr, width - .5, height / 2 + .5);
        cairo_set_source(cr, yellow);
        cairo_stroke(cr);
    }
    
}

int get_tic(struct processing_buffers *p)
{
    return p->tic;
}

int get_toc(struct processing_buffers *p)
{
    return p->toc;
}

double get_tic_pulse(struct processing_buffers *p)
{
    return p->tic_pulse;
}

double get_toc_pulse(struct processing_buffers *p)
{
    return p->toc_pulse;
}

gboolean info_draw_event(GtkWidget *widget, cairo_t *cr, struct main_window *w)
{
    cairo_init(cr);
    
    int old;
    struct processing_buffers *p = get_data(w, &old);
    
    double x = draw_watch_icon(cr, w->signal);
    
    char outputs[8][20];
    
    if(p) {
        int bph = w->guessed_bph;
        int rate = round(get_rate(bph, w->sample_rate, p));
        double be = fabs(p->be) * 1000 / p->sample_rate;
        double amp = get_amplitude(w->la, p);
        char rates[20];
        sprintf(rates,"%s%d",rate > 0 ? "+" : rate < 0 ? "-" : "",abs(rate));
        sprintf(outputs[0],"%4s",rates);
        sprintf(outputs[2]," %4.1f",be);
        if(amp > 0)
            sprintf(outputs[4]," %3.0f",amp);
        else
            strcpy(outputs[4]," ---");
    } else {
        strcpy(outputs[0],"----");
        strcpy(outputs[2]," ----");
        strcpy(outputs[4]," ---");
    }
    sprintf(outputs[6]," %d",w->guessed_bph);
    
    strcpy(outputs[1]," s/d");
    strcpy(outputs[3]," ms");
    strcpy(outputs[5]," deg");
    strcpy(outputs[7]," bph");
    
    cairo_text_extents_t extents;
    
    cairo_set_font_size(cr, OUTPUT_FONT);
    cairo_text_extents(cr,"0", &extents);
    double y = (double)OUTPUT_WINDOW_HEIGHT/2 - extents.y_bearing - extents.height/2;
    
    int i;
    for(i=0; i <8; i++) {
        if(i%2) {
            cairo_set_source(cr, white);
            cairo_move_to(cr,x,y);
            cairo_set_font_size(cr, OUTPUT_FONT*2/3);
            cairo_show_text(cr,outputs[i]);
            cairo_text_extents(cr,outputs[i],&extents);
            x += extents.x_advance;
        } else {
            cairo_set_source(cr, i > 4 || !p || !old ? white : yellow);
            cairo_set_font_size(cr, OUTPUT_FONT);
            x = print_number(cr,x,y,outputs[i]);
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
            cairo_move_to(c,x,y);
            cairo_show_text(c,s);
            g_timer_reset(timer);
        }
    }
#endif
    
    return FALSE;
}

gboolean tic_draw_event(GtkWidget *widget, cairo_t *cr, struct main_window *w)
{
    draw_waveform(w, w->tic_drawing_area, cr, get_tic, get_tic_pulse);
    return FALSE;
}

gboolean toc_draw_event(GtkWidget *widget, cairo_t *cr, struct main_window *w)
{
    draw_waveform(w, w->toc_drawing_area, cr, get_toc, get_toc_pulse);
    return FALSE;
}

gboolean period_draw_event(GtkWidget *widget, cairo_t *cr, struct main_window *w)
{
    cairo_init(cr);
    
    int width = gtk_widget_get_allocated_width(w->period_drawing_area);
    int height = gtk_widget_get_allocated_height(w->period_drawing_area);
    
    int old;
    struct processing_buffers *p = get_data(w, &old);
    
    double toc, a=0, b=0;
    
    if(p) {
        toc = p->tic < p->toc ? p->toc : p->toc + p->period;
        a = ((double)p->tic + toc)/2 - p->period/2;
        b = ((double)p->tic + toc)/2 + p->period/2;
        
        cairo_move_to(cr, (p->tic - a - NEGATIVE_SPAN*.001*w->sample_rate) * width/p->period, 0);
        cairo_line_to(cr, (p->tic - a - NEGATIVE_SPAN*.001*w->sample_rate) * width/p->period, height);
        cairo_line_to(cr, (p->tic - a + POSITIVE_SPAN*.001*w->sample_rate) * width/p->period, height);
        cairo_line_to(cr, (p->tic - a + POSITIVE_SPAN*.001*w->sample_rate) * width/p->period, 0);
        cairo_set_source(cr, blueish);
        cairo_fill(cr);
        
        cairo_move_to(cr, (toc - a - NEGATIVE_SPAN*.001*w->sample_rate) * width/p->period, 0);
        cairo_line_to(cr, (toc - a - NEGATIVE_SPAN*.001*w->sample_rate) * width/p->period, height);
        cairo_line_to(cr, (toc - a + POSITIVE_SPAN*.001*w->sample_rate) * width/p->period, height);
        cairo_line_to(cr, (toc - a + POSITIVE_SPAN*.001*w->sample_rate) * width/p->period, 0);
        cairo_set_source(cr, blueish);
        cairo_fill(cr);
    }
    
    int i;
    for(i = 1; i < 16; i++) {
        int x = i * width / 16;
        cairo_move_to(cr, x+.5, .5);
        cairo_line_to(cr, x+.5, height - .5);
        if(i % 4)
            cairo_set_source(cr, green);
        else
            cairo_set_source(cr, red);
        cairo_stroke(cr);
    }
    
    if(p) {
        draw_graph(a,b,cr,p,w->period_drawing_area);
        
        cairo_set_source(cr, old?yellow:white);
        cairo_stroke_preserve(cr);
        cairo_fill(cr);
    } else {
        cairo_move_to(cr, .5, height / 2 + .5);
        cairo_line_to(cr, width - .5, height / 2 + .5);
        cairo_set_source(cr, yellow);
        cairo_stroke(cr);
    }
    
    return FALSE;
}

extern volatile uint64_t timestamp;

gboolean paperstrip_draw_event(GtkWidget *widget, cairo_t *cr, struct main_window *w)
{
    int i,old;
    struct processing_buffers *p = get_data(w, &old);
#ifdef LIGHT
    uint64_t time = timestamp / 2;
#else
    uint64_t time = timestamp;
#endif
    if(p && !old) {
        uint64_t last = w->events[w->events_wp];
        for(i=0; i<EVENTS_MAX && p->events[i]; i++)
            if(p->events[i] > last + floor(p->period / 4)) {
                if(++w->events_wp == EVENTS_COUNT) w->events_wp = 0;
                w->events[w->events_wp] = p->events[i];
                debug("event at %llu\n", w->events[w->events_wp]);
            }
        w->events_from = p->timestamp - ceil(p->period);
    } else {
        w->events_from = time;
    }
    
    cairo_init(cr);
    
    int width = gtk_widget_get_allocated_width(w->paperstrip_drawing_area);
    int height = gtk_widget_get_allocated_height(w->paperstrip_drawing_area);
    
    int stopped = 0;
    if(w->events[w->events_wp] && time > 5 * w->sample_rate + w->events[w->events_wp]) {
        time = 5 * w->sample_rate + w->events[w->events_wp];
        stopped = 1;
    }
    
    int strip_width = round(width / (1 + PAPERSTRIP_MARGIN));
    
    cairo_set_line_width(cr, 1.3);
    
    if(p && w->events[w->events_wp]) {
        double rate = get_rate(w->guessed_bph, w->sample_rate, p);
        double slope = - rate * strip_width * PAPERSTRIP_ZOOM / (3600. * 24.);
        if(slope <= 1 && slope >= -1) {
            for(i=0; i<4; i++) {
                double y = 0;
                cairo_move_to(cr, (double)width * (i+.5) / 4, 0);
                for(;;) {
                    double x = y * slope + (double)width * (i+.5) / 4;
                    x = fmod(x, width);
                    if(x < 0) x += width;
                    double nx = x + slope * (height - y);
                    if(nx >= 0 && nx <= width) {
                        cairo_line_to(cr, nx, height);
                        break;
                    } else {
                        double d = slope > 0 ? width - x : x;
                        y += d / fabs(slope);
                        cairo_line_to(cr, slope > 0 ? width : 0, y);
                        y += 1;
                        if(y > height) break;
                        cairo_move_to(cr, slope > 0 ? 0 : width, y);
                    }
                }
            }
            cairo_set_source(cr, blue);
            cairo_stroke(cr);
        }
    }
    
    cairo_set_line_width(cr,1);
    
    int left_margin = (width - strip_width) / 2;
    int right_margin = (width + strip_width) / 2;
    cairo_move_to(cr, left_margin + .5, .5);
    cairo_line_to(cr, left_margin + .5, height - .5);
    cairo_move_to(cr, right_margin + .5, .5);
    cairo_line_to(cr, right_margin + .5, height - .5);
    cairo_set_source(cr, green);
    cairo_stroke(cr);
    
    double sweep = w->sample_rate * 3600. / w->guessed_bph;
    double now = sweep*ceil(time/sweep);
    double ten_s = w->sample_rate * 10 / sweep;
    double last_line = fmod(now/sweep, ten_s);
    int last_tenth = floor(now/(sweep*ten_s));
    for(i=0;;i++) {
        double y = 0.5 + round(last_line + i*ten_s);
        if(y > height) break;
        cairo_move_to(cr, .5, y);
        cairo_line_to(cr, width-.5, y);
        cairo_set_source(cr, (last_tenth-i)%6 ? green : red);
        cairo_stroke(cr);
    }
    
    cairo_set_source(cr, stopped?yellow:white);
    for(i = w->events_wp;;) {
        if(!w->events[i]) break;
        double event = now - w->events[i] + w->trace_centering + sweep * PAPERSTRIP_MARGIN / (2 * PAPERSTRIP_ZOOM);
        int column = floor(fmod(event, (sweep / PAPERSTRIP_ZOOM)) * strip_width / (sweep / PAPERSTRIP_ZOOM));
        int row = floor(event / sweep);
        if(row >= height) break;
        cairo_move_to(cr,column,row);
        cairo_line_to(cr,column+1,row);
        cairo_line_to(cr,column+1,row+1);
        cairo_line_to(cr,column,row+1);
        cairo_line_to(cr,column,row);
        cairo_fill(cr);
        if(column < width - strip_width && row > 0) {
            column += strip_width;
            row -= 1;
            cairo_move_to(cr,column,row);
            cairo_line_to(cr,column+1,row);
            cairo_line_to(cr,column+1,row+1);
            cairo_line_to(cr,column,row+1);
            cairo_line_to(cr,column,row);
            cairo_fill(cr);
        }
        if(--i < 0) i = EVENTS_COUNT - 1;
        if(i == w->events_wp) break;
    }
    
    // Draw the arrowed line for the ms scale at the bottom
    cairo_set_source(cr, white);
    cairo_set_line_width(cr, 2);
    cairo_move_to(cr, left_margin + 3, height - 20.5);
    cairo_line_to(cr, right_margin - 3, height - 20.5);
    cairo_stroke(cr);
    cairo_set_line_width(cr,1);
    cairo_move_to(cr, left_margin + .5, height - 20.5);
    cairo_line_to(cr, left_margin + 5.5, height - 15.5);
    cairo_line_to(cr, left_margin + 5.5, height - 25.5);
    cairo_line_to(cr, left_margin + .5, height - 20.5);
    cairo_fill(cr);
    cairo_move_to(cr, right_margin + .5, height - 20.5);
    cairo_line_to(cr, right_margin - 4.5, height - 15.5);
    cairo_line_to(cr, right_margin - 4.5, height - 25.5);
    cairo_line_to(cr, right_margin + .5, height - 20.5);
    cairo_fill(cr);
    
    // Write the ms scale at the bottom of the paperstrip
    
    int fontsize = gtk_widget_get_allocated_width(w->window) / 90;
    if(fontsize < 12)
        fontsize = 12;
    cairo_set_font_size(cr, fontsize);
    
    char s[100];
    cairo_text_extents_t extents;
    
    sprintf(s, "%.1f ms", 3600000. / (w->guessed_bph * PAPERSTRIP_ZOOM));
    cairo_text_extents(cr,s,&extents);
    cairo_move_to(cr, (width - extents.x_advance)/2, height - 30);
    cairo_show_text(cr, s);
    
    return FALSE;
}

#ifdef DEBUG
gboolean debug_draw_event(GtkWidget *widget, cairo_t *cr, struct main_window *w)
{
    int old = 0;
    struct processing_buffers *p = get_data(w, &old);
    
    if(p) {
        double a = p->period / 10;
        double b = p->period * 2;
        
        cairo_init(cr);
        
        draw_debug_graph(a,b,cr,p,w->debug_drawing_area);
        
        cairo_set_source(cr, old?yellow:white);
        cairo_stroke(cr);
    }
    
    return FALSE;
}
#endif

/* Called when the user changes the bph box value */
void handle_bph_change(GtkComboBox *b, struct main_window *w)
{
    char *s = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(b));
    if(s) {
        int n;
        char *t;
        n = (int)strtol(s,&t,10);
        if(*t || n < MIN_BPH || n > MAX_BPH) w->bph = 0;
        else w->bph = w->guessed_bph = n;
        g_free(s);
        recompute(w);
        redraw(w);
    }
}

/* Called when the user changes the lift angle value */
void handle_la_change(GtkSpinButton *b, struct main_window *w)
{
    double la = gtk_spin_button_get_value(b);
    if(la < MIN_LA || la > MAX_LA) la = DEFAULT_LA;
    w->la = la;
    redraw(w);
}

/* Called when the user clicks the Clear button */
void handle_clear_trace(GtkButton *b, struct main_window *w)
{
    memset(w->events, 0, EVENTS_COUNT*sizeof(uint64_t));
    redraw(w);
}

/* Called when the user clicks the Center button */
void handle_center_trace(GtkButton *b, struct main_window *w)
{
    uint64_t last_ev = w->events[w->events_wp];
    if(last_ev) {
        double sweep = w->sample_rate * 3600. / (PAPERSTRIP_ZOOM * w->guessed_bph);
        w->trace_centering = fmod(last_ev + .5*sweep , sweep);
    } else
        w->trace_centering = 0;
    redraw(w);
}

void quit()
{
    gtk_main_quit();
}

/* Set up the main window and populate with widgets */
void init_main_window(struct main_window *w)
{
    w->signal = 0;
    
    w->events = malloc(EVENTS_COUNT * sizeof(uint64_t));
    memset(w->events,0,EVENTS_COUNT * sizeof(uint64_t));
    w->events_wp = 0;
    w->events_from = 0;
    w->trace_centering = 0;
    
    w->guessed_bph = w->last_bph = DEFAULT_BPH;
    w->bph = 0;
    w->la = DEFAULT_LA;
    
    gtk_container_set_border_width(GTK_CONTAINER(w->window), 5); // Border around the window
    g_signal_connect(w->window, "delete_event", G_CALLBACK(delete_event), NULL);
    g_signal_connect(w->window, "destroy", G_CALLBACK(quit), w);
    
    gtk_window_set_title(GTK_WINDOW(w->window), PROGRAM_NAME " " VERSION);
    
    // Populate the settings grid
    GtkWidget *settings_grid = gtk_grid_new(); // The grid containing the settings, default to horizontal orientation
    gtk_grid_set_column_spacing(GTK_GRID(settings_grid), 2);
    
    // Beat mode Label
    GtkWidget *bph_label = gtk_label_new("Beat mode");
    gtk_container_add (GTK_CONTAINER(settings_grid), bph_label); // Add to grid
    
    // BPH combo box
    w->bph_combo_box = gtk_combo_box_text_new_with_entry();
    // Fill in pre-defined values
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box), "Auto");
    int *bph;
    for(bph = preset_bph; *bph; bph++) {
        char s[100];
        sprintf(s,"%d", *bph);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(w->bph_combo_box), s);
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(w->bph_combo_box), 0);
    gtk_widget_set_can_default(w->bph_combo_box, FALSE); // Try to avoid getting the automatic focus
    g_signal_connect (w->bph_combo_box, "changed", G_CALLBACK(handle_bph_change), w);
    gtk_container_add (GTK_CONTAINER(settings_grid), w->bph_combo_box);
    
    // Lift angle label
    GtkWidget *la_label = gtk_label_new("Lift angle");
    gtk_widget_set_margin_start(la_label, 10); // Make space from the widget in front
    gtk_container_add (GTK_CONTAINER(settings_grid), la_label);
    
    // Lift angle spin button
    w->la_spin_button = gtk_spin_button_new_with_range(MIN_LA, MAX_LA, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(w->la_spin_button), DEFAULT_LA); // Start at default value
    g_signal_connect (w->la_spin_button, "value_changed", G_CALLBACK(handle_la_change), w);
    gtk_container_add (GTK_CONTAINER(settings_grid), w->la_spin_button);
    
#ifdef DEBUG
    w->debug_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(w->debug_drawing_area, 500, 100);
    g_signal_connect (w->debug_drawing_area, "draw", G_CALLBACK(debug_draw_event), w);
    gtk_widget_set_events(w->debug_drawing_area, GDK_EXPOSURE_MASK);
    
    gtk_container_add (GTK_CONTAINER(settings_grid), w->debug_drawing_area);
    printf("DEBUG!\n");
#endif
    
    // Info area
    w->info_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(w->info_drawing_area, 720, OUTPUT_WINDOW_HEIGHT);
    g_signal_connect (w->info_drawing_area, "draw", G_CALLBACK(info_draw_event), w);
    gtk_widget_set_events(w->info_drawing_area, GDK_EXPOSURE_MASK);
    
    // Populate the panes
    GtkWidget *panes = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_wide_handle(GTK_PANED(panes), TRUE);
    
    /* Clunky way to make the pane handle bigger, but seems to be the only option
     otherwise the max is 5px with  gtk_paned_set_wide_handle() */
    GtkStyleContext *sc = gtk_widget_get_style_context(panes);
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider, "* {\n"
                                    "    -GtkPaned-handle-size: 10;\n"
                                    " }\n"
                                    , -1, NULL);
    
    gtk_style_context_add_provider(sc, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref (provider);
    
    
    GtkWidget *left_grid = gtk_grid_new();
    GtkWidget *right_grid = gtk_grid_new();
    
    // Set direction of the right grid since we're using gtk_container_add() to add contents
    gtk_orientable_set_orientation (GTK_ORIENTABLE (right_grid), GTK_ORIENTATION_VERTICAL);

    gtk_grid_set_row_spacing(GTK_GRID(left_grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(right_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(left_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(right_grid), 5);
    
    // The 3 waveforms on the right pane should all be equal height
    gtk_grid_set_row_homogeneous(GTK_GRID(right_grid), TRUE);
    
    // Add the grids to the two panes
    gtk_paned_pack1(GTK_PANED(panes), left_grid, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(panes), right_grid, TRUE, FALSE);
    
    gtk_widget_set_size_request(left_grid, 100, -1); // Minimum size of left pane
    gtk_widget_set_size_request(right_grid, 200, 300); // Minimum size of right pane
    
    // Paperstrip
    w->paperstrip_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(w->paperstrip_drawing_area, 100, 300); // Min width is actually limited by the buttons (~150px)
    g_signal_connect (w->paperstrip_drawing_area, "draw", G_CALLBACK(paperstrip_draw_event), w);
    gtk_widget_set_events(w->paperstrip_drawing_area, GDK_EXPOSURE_MASK);
    gtk_widget_set_hexpand(w->paperstrip_drawing_area, TRUE); // Make sure we expand when pane resizes
    gtk_widget_set_vexpand(w->paperstrip_drawing_area, TRUE);
    gtk_grid_attach(GTK_GRID(left_grid), w->paperstrip_drawing_area, 0,0,2,1);
    
    // CLEAR button
    GtkWidget *clear_button = gtk_button_new_with_label("Clear");
    gtk_container_set_border_width (GTK_CONTAINER(clear_button), 2);
    g_signal_connect (clear_button, "clicked", G_CALLBACK(handle_clear_trace), w);
    gtk_grid_attach(GTK_GRID(left_grid), clear_button, 0,1,1,1);
    
    // CENTER button
    GtkWidget *center_button = gtk_button_new_with_label("Center");
    gtk_container_set_border_width (GTK_CONTAINER(center_button), 2);
    g_signal_connect (center_button, "clicked", G_CALLBACK(handle_center_trace), w);
    gtk_grid_attach(GTK_GRID(left_grid), center_button, 1,1,1,1);
    
    // Tic waveform area
    w->tic_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(w->tic_drawing_area, 400, 100);
    g_signal_connect (w->tic_drawing_area, "draw", G_CALLBACK(tic_draw_event), w);
    gtk_widget_set_events(w->tic_drawing_area, GDK_EXPOSURE_MASK);
    gtk_widget_set_hexpand(w->tic_drawing_area, TRUE); // Make sure we expand when pane resizes
    gtk_widget_set_vexpand(w->tic_drawing_area, TRUE);
    gtk_container_add (GTK_CONTAINER(right_grid), w->tic_drawing_area);
    
    // Toc waveform area
    w->toc_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(w->toc_drawing_area, 400, 100);
    g_signal_connect (w->toc_drawing_area, "draw", G_CALLBACK(toc_draw_event), w);
    gtk_widget_set_events(w->toc_drawing_area, GDK_EXPOSURE_MASK);
    gtk_widget_set_hexpand(w->toc_drawing_area, TRUE); // Make sure we expand when pane resizes
    gtk_widget_set_vexpand(w->toc_drawing_area, TRUE);
    gtk_container_add (GTK_CONTAINER(right_grid), w->toc_drawing_area);
    
    // Period waveform area
    w->period_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(w->period_drawing_area, 400, 100);
    g_signal_connect (w->period_drawing_area, "draw", G_CALLBACK(period_draw_event), w);
    gtk_widget_set_events(w->period_drawing_area, GDK_EXPOSURE_MASK);
    gtk_widget_set_hexpand(w->period_drawing_area, TRUE); // Make sure we expand when pane resizes
    gtk_widget_set_vexpand(w->period_drawing_area, TRUE);
    gtk_container_add (GTK_CONTAINER(right_grid), w->period_drawing_area);
    
    
    // Populate the root grid with the grids we created above
    GtkWidget *root_grid = gtk_grid_new(); // The grid containing all of the UI
    gtk_orientable_set_orientation (GTK_ORIENTABLE (root_grid), GTK_ORIENTATION_VERTICAL);
    gtk_grid_set_row_spacing(GTK_GRID(root_grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(root_grid), 10);
    gtk_container_add(GTK_CONTAINER(w->window), root_grid); // Add the root grid to the window
    
    gtk_container_add (GTK_CONTAINER(root_grid), settings_grid);
    gtk_container_add (GTK_CONTAINER(root_grid), w->info_drawing_area);
    gtk_container_add (GTK_CONTAINER(root_grid), panes);
    
    // All done. Show all the widgets.
    gtk_widget_show_all (w->window);
    gtk_window_set_focus (GTK_WINDOW(w->window), NULL); // Unsets the focus widget (not working atm)

    // gtk_window_maximize(GTK_WINDOW(w->window));
}


/* Called when the application starts running */
static void activate (GtkApplication* app, gpointer user_data)
{
    initialize_palette(); // Set up the color definitions we'll be using
    
    int nominal_sr;
    double real_sr;
    
    // Initialize audio
    if (start_portaudio(&nominal_sr, &real_sr)) return; // Bail out if we can't open audio.
    
    struct processing_buffers p[NSTEPS];
    int i;
    for(i=0; i < NSTEPS; i++) {
        p[i].sample_rate = nominal_sr;
        p[i].sample_count = nominal_sr * (1<<(i+FIRST_STEP));
        setup_buffers(&p[i]);
        p[i].period = -1;
    }
    
    // Initialize the "global" w object
    struct main_window w;
    w.sample_rate = real_sr;
    w.bfs = p;
    w.old = NULL;
    w.window = gtk_application_window_new (app);
    
    // Set up GDK+ widgets for the UI
    init_main_window(&w);
    
    // Call refresh_window() 10 times/second
    g_timeout_add_full(G_PRIORITY_LOW, 100, (GSourceFunc)refresh_window, &w, NULL);
    
    gtk_main(); // Runs the main loop until gtk_main_quit() is called.
    
}

/* PROGRAM START */
int main(int argc, char **argv)
{
    GtkApplication *app;
    int status;
    
    app = gtk_application_new ("li.ciovil.tg", G_APPLICATION_FLAGS_NONE); // TODO: app id?
    
    g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
    status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);
    
    return status;
}
