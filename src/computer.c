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

int count_events(struct snapshot *s)
{
	int i, cnt = 0;
	if(!s->events_count) return 0;
	for(i = s->events_wp; s->events[i];) {
		cnt++;
		if(--i < 0) i = s->events_count - 1;
		if(i == s->events_wp) break;
	}
	return cnt;
}

struct snapshot *snapshot_clone(struct snapshot *s)
{
	struct snapshot *t = malloc(sizeof(struct snapshot));
	memcpy(t,s,sizeof(struct snapshot));
	if(s->pb) t->pb = pb_clone(s->pb);
	t->events_count = count_events(s);
	if(t->events_count) {
		t->events_wp = t->events_count - 1;
		t->events = malloc(t->events_count * sizeof(uint64_t));
		int i, j;
		for(i = t->events_wp, j = s->events_wp; i >= 0; i--) {
			t->events[i] = s->events[j];
			if(--j < 0) j = s->events_count - 1;
		}
	} else {
		t->events_wp = 0;
		t->events = NULL;
	}
	return t;
}

void snapshot_destroy(struct snapshot *s)
{
	if(s->pb) pb_destroy_clone(s->pb);
	free(s->events);
	free(s);
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

void compute_update_cal(struct computer *c)
{
	c->actv->signal = analyze_pa_data_cal(c->pdata, c->cdata);
	if(c->actv->pb) {
		pb_destroy_clone(c->actv->pb);
		c->actv->pb = NULL;
	}
	c->actv->cal_state = c->cdata->state;
	c->actv->cal_percent = 100*c->cdata->wp/c->cdata->size;
	if(c->cdata->state == 1)
		c->actv->cal_result = round(10 * c->cdata->calibration);
}

void compute_update(struct computer *c)
{
	int signal = analyze_pa_data(c->pdata, c->actv->bph, c->actv->la, c->actv->events_from);
	struct processing_buffers *p = c->pdata->buffers;
	int i;
	for(i=0; i<NSTEPS && p[i].ready; i++);
	for(i--; i>=0 && p[i].sigma > p[i].period / 10000; i--);
	if(i>=0) {
		if(c->actv->pb) pb_destroy_clone(c->actv->pb);
		c->actv->pb = pb_clone(&p[i]);
		c->actv->is_old = 0;
		c->actv->signal = i == NSTEPS-1 && p[i].amp < 0 ? signal-1 : signal;
	} else {
		c->actv->is_old = 1;
		c->actv->signal = -signal;
	}
}

void compute_events_cal(struct computer *c)
{
	struct calibration_data *d = c->cdata;
	struct snapshot *s = c->actv;
	int i;
	for(i=d->wp-1; i >= 0 &&
		d->events[i] > s->events[s->events_wp];
		i--);
	for(i++; i<d->wp; i++) {
		if(d->events[i] / s->nominal_sr <= s->events[s->events_wp] / s->nominal_sr)
			continue;
		if(++s->events_wp == s->events_count) s->events_wp = 0;
		s->events[s->events_wp] = d->events[i];
		debug("event at %llu\n",s->events[s->events_wp]);
	}
	s->events_from = get_timestamp();
}

void compute_events(struct computer *c)
{
	struct snapshot *s = c->actv;
	struct processing_buffers *p = c->actv->pb;
	if(p && !s->is_old) {
		uint64_t last = s->events[s->events_wp];
		int i;
		for(i=0; i<EVENTS_MAX && p->events[i]; i++)
			if(p->events[i] > last + floor(p->period / 4)) {
				if(++s->events_wp == s->events_count) s->events_wp = 0;
				s->events[s->events_wp] = p->events[i];
				debug("event at %llu\n",s->events[s->events_wp]);
			}
		s->events_from = p->timestamp - ceil(p->period);
	} else {
		s->events_from = get_timestamp();
	}
}

void compute_results(struct snapshot *s)
{
	s->sample_rate = s->nominal_sr * (1 + (double) s->cal / (10 * 3600 * 24));
	if(s->pb) {
		s->guessed_bph = s->bph ? s->bph : guess_bph(s->pb->period / s->sample_rate);
		s->rate = (7200/(s->guessed_bph * s->pb->period / s->sample_rate) - 1)*24*3600;
		s->be = fabs(s->pb->be) * 1000 / s->sample_rate;
		s->amp = s->la * s->pb->amp; // 0 = not available
		if(s->amp < 135 || s->amp > 360)
			s->amp = 0;
	} else
		s->guessed_bph = s->bph ? s->bph : DEFAULT_BPH;
}

void *computing_thread(void *void_computer)
{
	struct computer *c = void_computer;
	for(;;) {
		pthread_mutex_lock(&c->mutex);
			while(!c->recompute)
				pthread_cond_wait(&c->cond, &c->mutex);
			if(c->recompute > 0) c->recompute = 0;
			int calibrate = c->calibrate;
			c->actv->bph = c->bph;
			c->actv->la = c->la;
			void (*callback)(void *) = c->callback;
			void *callback_data = c->callback_data;
		pthread_mutex_unlock(&c->mutex);

		if(c->recompute < 0) {
			if(callback) callback(callback_data);
			break;
		}

		if(calibrate && !c->actv->calibrate) {
			c->cdata->wp = 0;
			c->cdata->state = 0;
			c->actv->cal_state = 0;
			c->actv->cal_percent = 0;
		}
		if(calibrate != c->actv->calibrate)
			memset(c->actv->events,0,c->actv->events_count*sizeof(uint64_t));
		c->actv->calibrate = calibrate;

		if(c->actv->calibrate) {
			compute_update_cal(c);
			compute_events_cal(c);
		} else {
			compute_update(c);
			compute_events(c);
		}

		pthread_mutex_lock(&c->mutex);
			if(c->curr)
				snapshot_destroy(c->curr);
			if(c->clear_trace) {
				if(!calibrate)
					memset(c->actv->events,0,c->actv->events_count*sizeof(uint64_t));
				c->clear_trace = 0;
			}
			c->curr = snapshot_clone(c->actv);
		pthread_mutex_unlock(&c->mutex);

		if(callback) callback(callback_data);
	}

	debug("Terminating computation thread\n");

	return NULL;
}

void computer_destroy(struct computer *c)
{
	int i;
	for(i=0; i<NSTEPS; i++)
		pb_destroy(&c->pdata->buffers[i]);
	free(c->pdata->buffers);
	free(c->pdata);
	cal_data_destroy(c->cdata);
	free(c->cdata);
	snapshot_destroy(c->actv);
	if(c->curr)
		snapshot_destroy(c->curr);
	pthread_mutex_destroy(&c->mutex);
	pthread_cond_destroy(&c->cond);
	pthread_join(c->thread, NULL);
	free(c);
}

struct computer *start_computer(int nominal_sr, int bph, double la, int cal)
{
	struct processing_buffers *p = malloc(NSTEPS * sizeof(struct processing_buffers));
	int i;
	for(i=0; i<NSTEPS; i++) {
		p[i].sample_rate = nominal_sr;
		p[i].sample_count = nominal_sr * (1<<(i+FIRST_STEP));
		setup_buffers(&p[i]);
	}

	struct processing_data *pd = malloc(sizeof(struct processing_data));
	pd->buffers = p;
	pd->last_tic = 0;

	struct calibration_data *cd = malloc(sizeof(struct calibration_data));
	setup_cal_data(cd);

	struct snapshot *s = malloc(sizeof(struct snapshot));
	s->timestamp = 0;
	s->nominal_sr = nominal_sr;
	s->pb = NULL;
	s->is_old = 1;
	s->calibrate = 0;
	s->signal = 0;
	s->events_count = EVENTS_COUNT;
	s->events = malloc(EVENTS_COUNT * sizeof(uint64_t));
	memset(s->events,0,EVENTS_COUNT * sizeof(uint64_t));
	s->events_wp = 0;
	s->events_from = 0;
	s->trace_centering = 0;
	s->bph = bph;
	s->la = la;
	s->cal = cal;

	struct computer *c = malloc(sizeof(struct computer));
	c->cdata = cd;
	c->pdata = pd;
	c->actv = s;
	c->curr = snapshot_clone(s);
	c->recompute = 0;
	c->calibrate = 0;
	c->clear_trace = 0;

	if(    pthread_mutex_init(&c->mutex, NULL)
	    || pthread_cond_init(&c->cond, NULL)
	    || pthread_create(&c->thread, NULL, computing_thread, c)) {
		error("Unable to initialize computing thread");
		return NULL;
	}

	return c;
}

void lock_computer(struct computer *c)
{
	pthread_mutex_lock(&c->mutex);
}

void unlock_computer(struct computer *c)
{
	if(c->recompute)
		pthread_cond_signal(&c->cond);
	pthread_mutex_unlock(&c->mutex);
}
