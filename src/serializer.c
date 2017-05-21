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

int serialize_uint64_t(FILE *f, uint64_t x)
{
	return 0 > fprintf(f, "I%lu\n", x);
}

int scan_uint64_t(FILE *f, uint64_t *x)
{
	return 1 != fscanf(f, "I%lu\n", x);
}

int serialize_int64_t(FILE *f, int64_t x)
{
	return 0 > fprintf(f, "I%ld\n", x);
}

int scan_int64_t(FILE *f, int64_t *x)
{
	return 1 != fscanf(f, "I%ld\n", x);
}

int serialize_int(FILE *f, int x)
{
	return serialize_int64_t(f, x);
}

int scan_int(FILE *f, int *x)
{
	int64_t y;
	if(scan_int64_t(f, &y) || y < INT_MIN || y > INT_MAX) return 1;
	*x = y;
	return 0;
}

int serialize_double(FILE *f, double x)
{
	return 0 > fprintf(f, "I%a\n", x);
}

int scan_double(FILE *f, double *x)
{
	return 1 != fscanf(f, "I%la\n", x);
}

int serialize_float(FILE *f, float x)
{
	return serialize_double(f, x);
}

int scan_float(FILE *f, float *x)
{
	double y;
	if(scan_double(f, &y)) return 1;
	*x = y;
	return 0;
}

int serialize_string(FILE *f, char *s)
{
	if(0 > fprintf(f, "S%lu\n", (uint64_t)strlen(s))) return 1;
	return 0 > fprintf(f, "%s\n", s);
}

int scan_string(FILE *f, char **s, uint64_t max_l, uint64_t *len)
{
	uint64_t l;
	if(1 != fscanf(f, "S%lu\n", &l)) return 1;
	if(l >= max_l) return 1;
	if(!*s) *s = malloc(l+1);
	if(l+1 != fread(*s, 1, l+1, f)) return 1;
	if(*s[l] != '\n') return 1;
	*s[l] = 0;
	if(len) *len = l;
	return 0;
}

int serialize_uint64_t_array(FILE *f, uint64_t *a, uint64_t len)
{
	uint64_t i;
	if(0 > fprintf(f, "A%lu\n", len)) return 1;
	for(i = 0; i < len; i++)
		if(serialize_uint64_t(f, a[i])) return 1;
	return 0;
}

int scan_uint64_t_array(FILE *f, uint64_t **a, uint64_t max_l, uint64_t *len)
{
	uint64_t l,i;
	if(1 != fscanf(f, "A%lu\n", &l)) return 1;
	if(l > max_l) return 1;
	if(!*a) *a = malloc(l*sizeof(uint64_t));
	for(i = 0; i < l; i++)
		if(scan_uint64_t(f, *a+i)) return 1;
	if(len) *len = l;
	return 0;
}

int serialize_float_array(FILE *f, float *a, uint64_t len)
{
	uint64_t i;
	if(0 > fprintf(f, "A%lu\n", len)) return 1;
	for(i = 0; i < len; i++)
		if(serialize_float(f, a[i])) return 1;
	return 0;
}

int scan_float_array(FILE *f, float **a, uint64_t max_l, uint64_t *len)
{
	uint64_t l,i;
	if(1 != fscanf(f, "A%lu\n", &l)) return 1;
	if(l > max_l) return 1;
	if(!*a) *a = malloc(l*sizeof(float));
	for(i = 0; i < l; i++)
		if(scan_float(f, *a+i)) return 1;
	if(len) *len = l;
	return 0;
}

int make_label(FILE *f, char *l)
{
	return 0 > fprintf(f, "L%s\n", l);
}

int scan_label(FILE *f, char *l)
{
	return 1 != fscanf(f, "L%s\n", l);
}

#define SERIALIZE(T,A) {				\
	if(make_label(f, #A)) return 1;			\
	if(serialize_ ## T (f, s -> A)) return 1;	\
	}

int serialize_snapshot(FILE *f, struct snapshot *s)
{
	if(s->pb) {
		SERIALIZE(int,pb->sample_rate);
		SERIALIZE(int,pb->sample_count);
		SERIALIZE(double,pb->period);
		SERIALIZE(double,pb->waveform_max);
		SERIALIZE(int,pb->tic);
		SERIALIZE(int,pb->toc);
		SERIALIZE(double,pb->tic_pulse);
		SERIALIZE(double,pb->toc_pulse);
		if(make_label(f, "pb->waveform")) return 1;
		if(serialize_float_array(f, s->pb->waveform, s->pb->sample_count)) return 1;
	} else {
		if(make_label(f, "pb-null")) return 1;
		if(serialize_int(f, 0)) return 1;
	}
	SERIALIZE(int,is_old);
	SERIALIZE(uint64_t,timestamp);
	SERIALIZE(int,nominal_sr);
	SERIALIZE(int,bph);
	SERIALIZE(double,la);
	SERIALIZE(int,cal);
	SERIALIZE(int,events_wp);
	SERIALIZE(int,signal);
	SERIALIZE(double,sample_rate);
	SERIALIZE(int,guessed_bph);
	SERIALIZE(double,rate);
	SERIALIZE(double,be);
	SERIALIZE(double,amp);
	SERIALIZE(double,trace_centering);
	if(make_label(f, "events")) return 1;
	if(serialize_uint64_t_array(f, s->events, EVENTS_COUNT)) return 1;
	return 0;
}
