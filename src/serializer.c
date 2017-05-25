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
#include <inttypes.h>

#define LABEL_SIZE 64

#define XSTR(X) STR(X)
#define STR(X) #X
#define LABEL_SIZE_STR XSTR(LABEL_SIZE)

int serialize_uint64_t(FILE *f, uint64_t x)
{
	return 0 > fprintf(f, "I%"PRIu64";\n", x);
}

int scan_uint64_t(FILE *f, uint64_t *x)
{
	int n = 0;
	return 1 != fscanf(f, " I%"SCNu64";%n", x, &n) || !n;
}

int serialize_int64_t(FILE *f, int64_t x)
{
	return 0 > fprintf(f, "I%"PRId64";\n", x);
}

int scan_int64_t(FILE *f, int64_t *x)
{
	int n = 0;
	return 1 != fscanf(f, " I%"SCNd64";%n", x, &n) || !n;
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
	return 0 > fprintf(f, "I%a;\n", x);
}

int scan_double(FILE *f, double *x)
{
	int n = 0;
	return 1 != fscanf(f, " I%la;%n", x, &n) || !n;
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
	if(0 > fprintf(f, "S%"PRIu64";", (uint64_t)strlen(s))) return 1;
	return 0 > fprintf(f, "%s;\n", s);
}

int scan_string(FILE *f, char **s, uint64_t max_l, uint64_t *len)
{
	uint64_t l;
	int n = 0;
	if(1 != fscanf(f, " S%"SCNu64";%n", &l, &n) || !n) return 1;
	if(l >= max_l) return 1;
	if(!*s) *s = malloc(l+1);
	if(l+1 != fread(*s, 1, l+1, f)) return 1;
	if(*s[l] != ';') return 1;
	*s[l] = 0;
	if(len) *len = l;
	return 0;
}

int serialize_uint64_t_array(FILE *f, uint64_t *a, uint64_t len)
{
	uint64_t i;
	if(0 > fprintf(f, "A%"PRIu64";\n", len)) return 1;
	for(i = 0; i < len; i++)
		if(serialize_uint64_t(f, a[i])) return 1;
	return 0;
}

int scan_uint64_t_array(FILE *f, uint64_t **a, uint64_t max_l, uint64_t *len)
{
	uint64_t l,i;
	int n = 0;
	if(1 != fscanf(f, " A%"SCNu64";%n", &l, &n) || !n) return 1;
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
	if(0 > fprintf(f, "A%"PRIu64";\n", len)) return 1;
	for(i = 0; i < len; i++)
		if(serialize_float(f, a[i])) return 1;
	return 0;
}

int scan_float_array(FILE *f, float **a, uint64_t max_l, uint64_t *len)
{
	uint64_t l,i;
	int n = 0;
	if(1 != fscanf(f, " A%"SCNu64";%n", &l, &n) || !n) return 1;
	if(l > max_l) return 1;
	if(!*a) *a = malloc(l*sizeof(float));
	for(i = 0; i < l; i++)
		if(scan_float(f, *a+i)) return 1;
	if(len) *len = l;
	return 0;
}

int make_label(FILE *f, char *l)
{
	return 0 > fprintf(f, "L%s;\n", l);
}

int scan_label(FILE *f, char *l)
{
	int n = 0;
	return 1 != fscanf(f, " L%" LABEL_SIZE_STR "[^;];%n", l, &n) || !n;
}

int eat_object(FILE *f)
{
	char c;
	if(1 != fscanf(f, " %c", &c)) return 1;
	uint64_t l;
	int n = 0;
	switch(c) {
		case 'I':
			fscanf(f, "%*[^;];%n", &n);
			return !n;
		case 'S':
			if(1 != fscanf(f, "%"SCNu64";%n", &l, &n) || !n) return 1;
			if(fseek(f, l, SEEK_CUR)) return 1;
			return 1 != fscanf(f, "%c", &c) || c != ';';
		case 'A':
			if(1 != fscanf(f, "%"SCNu64";%n", &l, &n) || !n) return 1;
			for(;l;l--) if(eat_object(f)) return 1;
			return 0;
		case 'T':
			fscanf(f, "%c", &c);
			if(c != ';') return 1;
			for(;;) {
				char b[LABEL_SIZE+1];
				if(scan_label(f, b)) return 1;
				if(!strcmp("__end__", b)) return 0;
				if(eat_object(f)) return 1;
			}
		default:
			return 1;
	}
}

int serialize_struct_begin(FILE *f)
{
	return 0 > fprintf(f, "T;\n");
}

int serialize_struct_end(FILE *f)
{
	return make_label(f, "__end__");
}

#define SERIALIZE(T,A) {				\
	if(make_label(f, #A)) return 1;			\
	if(serialize_ ## T (f, s -> A)) return 1;	\
	}

int serialize_snapshot(FILE *f, struct snapshot *s, char *name)
{
	if(make_label(f, "snapshot")) return 1;
	if(serialize_struct_begin(f)) return 1;
	if(make_label(f, "name")) return 1;
	if(serialize_string(f, name)) return 1;
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
	return serialize_struct_end(f);
}

int scan_snapshot(FILE *f, struct snapshot **s, char **name)
{
	char l[LABEL_SIZE+1];
	if(scan_label(f,l) || strcmp("snapshot",l)) return 1;
	int n = 0;
	fscanf(f, " T;%n", &n);
	if(!n) return 1;
	for(;;) {
		if(scan_label(f,l)) return 1;
		if(!strcmp("__end__",l)) break;
		if(eat_object(f)) return 1;
	}
	return 0;
}
