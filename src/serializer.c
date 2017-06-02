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

#ifdef _WIN32
int parse_hex_digit(char c)
{
	if('0' <= c && c <= '9') return c - '0';
	if('a' <= c && c <= 'f') return 10 + c - 'a';
	if('A' <= c && c <= 'F') return 10 + c - 'A';
	return -1;
}

int parse_hex_double(FILE *f, double *d)
{
	char s[64], *t = s;
	if(1 != fscanf(f, "%63[0-9a-fA-F.xXpP+-]", s)) return 1;
	double place = 1;
	if(*t == '+') t++;
	else if(*t == '-') {
		place = -1;
		t++;
	}
	if(*t != '0') return 1;
	if(*++t != 'x' && *t != 'X') return 1;
	int n = parse_hex_digit(*++t);
	if(n < 0) return 1;
	*d = place * n;
	if(!*++t) return 0;
	if(*t != '.') goto exponent;
	while(*++t && *t != 'p' && *t != 'P') {
		place /= 16;
		n = parse_hex_digit(*t);
		if(n < 0) return 1;
		*d += place * n;
	}
exponent:
	if(*t) {
		long int radix = strtol(t+1, &t, 10);
		if(*t) return 1;
		*d *= pow(2, radix);
	}
	return 0;
}
#endif

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
#ifdef _WIN32
	return 0 > __mingw_fprintf(f, "I%a;\n", x);
#else
	return 0 > fprintf(f, "I%a;\n", x);
#endif
}

int scan_double(FILE *f, double *x)
{
	int n = 0;
#ifdef _WIN32
	if( 0 != fscanf(f, " I%n", &n) || !n ) return 1;
	if( parse_hex_double(f,x) ) return 1;
	n = 0;
	if( 0 != fscanf(f, ";%n", &n) || !n ) return 1;
	return 0;
#else
	return 1 != fscanf(f, " I%la;%n", x, &n) || !n;
#endif
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
	if(max_l && l >= max_l) return 1;
	if(!*s) *s = malloc(l+1);
	if(l+1 != fread(*s, 1, l+1, f)) return 1;
	if((*s)[l] != ';') return 1;
	(*s)[l] = 0;
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
	if(max_l && l > max_l) return 1;
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
	if(max_l && l > max_l) return 1;
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
			return 0 != fscanf(f, "%*[^;];%n", &n) || !n;
		case 'S':
			if(1 != fscanf(f, "%"SCNu64";%n", &l, &n) || !n) return 1;
			if(fseek(f, l, SEEK_CUR)) return 1;
			return 1 != fscanf(f, "%c", &c) || c != ';';
		case 'A':
			if(1 != fscanf(f, "%"SCNu64";%n", &l, &n) || !n) return 1;
			for(;l;l--) if(eat_object(f)) return 1;
			return 0;
		case 'T':
			if(1 != fscanf(f, "%c", &c)) return 1;
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
	if(serialize_struct_begin(f)) return 1;
	if(name) {
		if(make_label(f, "name")) return 1;
		if(serialize_string(f, name)) return 1;
	}
	if(make_label(f, "pb->waveform")) return 1;
	if(serialize_float_array(f, s->pb->waveform, s->pb->sample_count)) return 1;
	if(make_label(f, "events")) return 1;
	if(serialize_uint64_t_array(f, s->events, s->events_count)) return 1;
	SERIALIZE(int,pb->sample_rate);
	SERIALIZE(double,pb->period);
	SERIALIZE(double,pb->waveform_max);
	SERIALIZE(int,pb->tic);
	SERIALIZE(int,pb->toc);
	SERIALIZE(double,pb->tic_pulse);
	SERIALIZE(double,pb->toc_pulse);
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
	return serialize_struct_end(f);
}

#define SCAN(T,A) {						\
	if(!strcmp( #A , l )) {					\
		debug("serializer: scanning " #A "\n");		\
		if(scan_ ## T (f, &((*s) -> A))) goto error;	\
		continue;					\
	}}

int scan_snapshot(FILE *f, struct snapshot **s, char **name)
{
	*s = malloc(sizeof(struct snapshot));
	memset(*s, 0, sizeof(struct snapshot));
	(*s)->pb = malloc(sizeof(struct processing_buffers));
	memset((*s)->pb, 0, sizeof(struct processing_buffers));
	*name = NULL;

	char l[LABEL_SIZE+1];
	int n = 0;
	if(0 != fscanf(f, " T;%n", &n) || !n) goto error;;
	for(;;) {
		if(scan_label(f,l)) goto error;
		if(!strcmp("__end__", l)) break;

		if(!strcmp("name", l)) {
			debug("serializer: scanning name\n");
			if(scan_string(f, name, 0, NULL)) goto error;
			continue;
		}
		if(!strcmp("pb->waveform", l)) {
			debug("serializer: scanning pb->waveform\n");
			uint64_t x;
			if(	(*s)->pb->waveform ||
				scan_float_array(f, &((*s)->pb->waveform), INT_MAX, &x)) goto error;
			(*s)->pb->sample_count = x;
			continue;
		}
		if(!strcmp("events", l)) {
			debug("serializer: scanning events\n");
			uint64_t x;
			if(	(*s)->events ||
				scan_uint64_t_array(f, &((*s)->events), INT_MAX, &x)) goto error;
			(*s)->events_count = x;
			continue;
		}
		SCAN(int,pb->sample_rate);
		SCAN(double,pb->period);
		SCAN(double,pb->waveform_max);
		SCAN(int,pb->tic);
		SCAN(int,pb->toc);
		SCAN(double,pb->tic_pulse);
		SCAN(double,pb->toc_pulse);
		SCAN(int,is_old);
		SCAN(uint64_t,timestamp);
		SCAN(int,nominal_sr);
		SCAN(int,bph);
		SCAN(double,la);
		SCAN(int,cal);
		SCAN(int,events_wp);
		SCAN(int,signal);
		SCAN(double,sample_rate);
		SCAN(int,guessed_bph);
		SCAN(double,rate);
		SCAN(double,be);
		SCAN(double,amp);
		SCAN(double,trace_centering);

		if(eat_object(f)) goto error;
	}
	debug("serializer: checking period\n");
	if((*s)->pb->period <= 0 || (*s)->pb->sample_count < ceil((*s)->pb->period)) goto error;
	debug("serializer: checking timestamp\n");
	if(!(*s)->timestamp) goto error;
	debug("serializer: checking nominal_sr\n");
	if(!(*s)->nominal_sr) goto error;
	if((*s)->bph < MIN_BPH || (*s)->bph > MAX_BPH) (*s)->bph = 0;
	if((*s)->la  < MIN_LA  || (*s)->la  > MAX_LA ) (*s)->la  = DEFAULT_LA;
	if((*s)->cal < MIN_CAL || (*s)->cal > MAX_CAL) (*s)->cal = 0;
	debug("serializer: checking events\n");
	if((*s)->events_count && (*s)->events_wp >= (*s)->events_count) goto error;
	if((*s)->signal > NSTEPS) (*s)->signal = NSTEPS;
	debug("serializer: checking sample_rate\n");
	if(!(*s)->sample_rate) goto error;
	// ...
	(*s)->pb->events = NULL;
#ifdef DEBUG
	(*s)->pb->debug = NULL;
#endif
	return 0;

error:
	free(*name);
	free((*s)->pb->waveform);
	free((*s)->pb);
	free((*s)->events);
	free(*s);
	*s = NULL;
	*name = NULL;
	return 1;
}

int scan_snapshot_list(FILE *f, struct snapshot ***s, char ***names, uint64_t *cnt)
{
	debug("serializer: scanning snapshot list\n");
	uint64_t i = 0;
	int n = 0;
	*s = NULL;
	*names = NULL;
	if(1 != fscanf(f, " A%"SCNu64";%n", cnt, &n) || !n) goto error;
	*s = malloc(*cnt*sizeof(struct snapshot *));
	*names = malloc(*cnt*sizeof(char *));
	for(i = 0; i < *cnt; i++)
		if(scan_snapshot(f, *s+i, *names+i)) goto error;
	return 0;
error:
	debug("serializer: error in snapshot list\n");
	for(; i--;) {
		snapshot_destroy((*s)[i]);
		free((*names)[i]);
	}
	free(*s);
	free(*names);
	*s = NULL;
	*names = NULL;
	return 1;
}

int write_file(FILE *f, struct snapshot **s, char **names, uint64_t cnt)
{
	uint64_t i;
	if(make_label(f, "tg-timer-version")) return 1;
	if(serialize_string(f, VERSION)) return 1;
	if(make_label(f, "data")) return 1;
	if(serialize_struct_begin(f)) return 1;
	if(make_label(f, "snapshot-list")) return 1;
	if(0 > fprintf(f, "A%"PRIu64";\n", cnt)) return 1;
	for(i = 0; i < cnt; i++)
		if(serialize_snapshot(f, s[i], names[i])) return 1;
	if(serialize_struct_end(f)) return 1;
	return 0;
}

int read_file(FILE *f, struct snapshot ***s, char ***names, uint64_t *cnt)
{
	debug("serializer: reading file\n");
	char lbl[LABEL_SIZE+1];
	char *l = lbl;
	if(scan_label(f, l) || strcmp("tg-timer-version",l)) return 1;
	if(scan_string(f, &l, LABEL_SIZE, NULL)) return 1;
	debug("serializer: read version %s\n",l);
	if(scan_label(f, l) || strcmp("data",l)) return 1;
	debug("serializer: found data structure\n",l);
	int n = 0;
	if(0 != fscanf(f, " T;%n", &n) || !n) return 1;
	*s = NULL;
	*names = NULL;
	for(;;) {
		if(scan_label(f,l)) goto error;
		if(!strcmp("__end__",l)) break;
		if(!strcmp("snapshot-list",l)) {
			if(*s || scan_snapshot_list(f, s, names, cnt)) goto error;
			else continue;
		}
		if(eat_object(f)) goto error;
	}
	debug("serializer: end of data structure\n",l);
	char c;
	if(*s && 1 != fscanf(f, " %c", &c)) return 0;
#ifdef DEBUG
	if(*s) {
		debug("serializer: stray char %c (%d) after end\n", c, c);
	} else {
		debug("serializer: no snapshots\n");
	}
#endif
error:
	debug("serializer: read error\n");
	if(*s) {
		int i;
		for(i = 0; i < *cnt; i++) {
			snapshot_destroy((*s)[i]);
			free((*names)[i]);
		}
		free(*s);
		free(*names);
		*s = NULL;
		*names = NULL;
	}
	return 1;
}
