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

struct filter {
	double a0,a1,a2,b1,b2;
};

int fl_cmp(const void *a, const void *b)
{
	float x = *(float*)a;
	float y = *(float*)b;
	return x<y ? -1 : x>y ? 1 : 0;
}

int int_cmp(const void *a, const void *b)
{
	int x = *(int*)a;
	int y = *(int*)b;
	return x<y ? -1 : x>y ? 1 : 0;
}

void make_hp(struct filter *f, double freq)
{
	double K = tan(M_PI * freq);
	double norm = 1 / (1 + K * sqrt(2) + K * K);
	f->a0 = 1 * norm;
	f->a1 = -2 * f->a0;
	f->a2 = f->a0;
	f->b1 = 2 * (K * K - 1) * norm;
	f->b2 = (1 - K * sqrt(2) + K * K) * norm;
}

void make_lp(struct filter *f, double freq)
{
	double K = tan(M_PI * freq);
	double norm = 1 / (1 + K * sqrt(2) + K * K);
	f->a0 = K * K * norm;
	f->a1 = 2 * f->a0;
	f->a2 = f->a0;
	f->b1 = 2 * (K * K - 1) * norm;
	f->b2 = (1 - K * sqrt(2) + K * K) * norm;
}

void run_filter(struct filter *f, float *buff, int size)
{
	int i;
	double z1 = 0, z2 = 0;
	for(i=0; i<size; i++) {
		double in = buff[i];
		double out = in * f->a0 + z1;
		z1 = in * f->a1 + z2 - f->b1 * out;
		z2 = in * f->a2 - f->b2 * out;
		buff[i] = out;
	}
}

void setup_buffers(struct processing_buffers *b)
{
	b->samples = fftwf_malloc(2 * b->sample_count * sizeof(float));
	b->samples_sc = malloc(2 * b->sample_count * sizeof(float));
	b->waveform = malloc(2 * b->sample_rate * sizeof(float));
	b->waveform_sc = malloc(2 * b->sample_rate * sizeof(float));
	b->fft = fftwf_malloc((b->sample_count + 1) * sizeof(fftwf_complex));
	b->sc_fft = fftwf_malloc((b->sample_count + 1) * sizeof(fftwf_complex));
	b->tic_wf = fftwf_malloc(b->sample_rate * sizeof(float));
	b->slice_wf = fftwf_malloc(b->sample_rate * sizeof(float));
	b->tic_fft = fftwf_malloc((b->sample_rate/2 + 1) * sizeof(fftwf_complex));
	b->slice_fft = fftwf_malloc((b->sample_rate/2 + 1) * sizeof(fftwf_complex));
	b->tic_c = malloc(2 * b->sample_count * sizeof(float));
	b->plan_a = fftwf_plan_dft_r2c_1d(2 * b->sample_count, b->samples, b->fft, FFTW_ESTIMATE);
	b->plan_b = fftwf_plan_dft_c2r_1d(2 * b->sample_count, b->sc_fft, b->samples_sc, FFTW_ESTIMATE);
	b->plan_c = fftwf_plan_dft_r2c_1d(2 * b->sample_rate, b->waveform, b->sc_fft, FFTW_ESTIMATE);
	b->plan_d = fftwf_plan_dft_c2r_1d(2 * b->sample_rate, b->sc_fft, b->waveform_sc, FFTW_ESTIMATE);
	b->plan_e = fftwf_plan_dft_r2c_1d(b->sample_rate, b->tic_wf, b->tic_fft, FFTW_ESTIMATE);
	b->plan_f = fftwf_plan_dft_r2c_1d(b->sample_rate, b->slice_wf, b->slice_fft, FFTW_ESTIMATE);
	b->plan_g = fftwf_plan_dft_c2r_1d(b->sample_rate, b->slice_fft, b->slice_wf, FFTW_ESTIMATE);
	b->hpf = malloc(sizeof(struct filter));
	make_hp(b->hpf,(double)FILTER_CUTOFF/b->sample_rate);
	b->lpf = malloc(sizeof(struct filter));
	make_lp(b->lpf,(double)FILTER_CUTOFF/b->sample_rate);
	b->events = malloc(EVENTS_MAX * sizeof(uint64_t));
	b->ready = 0;
#ifdef DEBUG
	b->debug = fftwf_malloc(b->sample_count * sizeof(float));
#endif
}

struct processing_buffers *pb_clone(struct processing_buffers *p)
{
	struct processing_buffers *new = malloc(sizeof(struct processing_buffers));
	new->sample_rate = p->sample_rate;
	new->waveform = malloc(new->sample_rate * sizeof(float));
	memcpy(new->waveform, p->waveform, new->sample_rate * sizeof(float));
	new->events = malloc(EVENTS_MAX * sizeof(uint64_t));
	memcpy(new->events, p->events, EVENTS_MAX * sizeof(uint64_t));

#ifdef DEBUG
	new->debug = malloc(p->sample_count * sizeof(float));
	memcpy(new->debug, p->debug, p->sample_count * sizeof(float));
#endif

	new->sample_count = p->sample_count;
	new->period = p->period;
	new->sigma = p->sigma;
	new->be = p->be;
	new->waveform_max = p->waveform_max;
	new->tic_pulse = p->tic_pulse;
	new->toc_pulse = p->toc_pulse;
	new->tic = p->tic;
	new->toc = p->toc;
	new->ready = p->ready;
	new->timestamp = p->timestamp;
	return new;
}

void pb_destroy_clone(struct processing_buffers *p)
{
	free(p->waveform);
	free(p->events);
#ifdef DEBUG
	free(p->debug);
#endif
	free(p);
}

float vmax(float *v, int a, int b, int *i_max)
{
	float max = v[a];
	if(i_max) *i_max = a;
	int i;
	for(i = a+1; i < b; i++) {
		if(v[i] > max) {
			max = v[i];
			if(i_max) *i_max = i;
		}
	}
	return max;
}

void noise_suppressor(struct processing_buffers *p)
{
	float *a = p->samples_sc;
	float *b = p->samples_sc + p->sample_count;
	int window = p->sample_rate / 50;
	int i;

	for(i = 0; i < p->sample_count; i++)
		a[i] = p->samples[i] * p->samples[i];

	double r_av = 0;
	for(i = 0; i < window; i++)
		r_av += a[i];
	for(i = 0;; i++) {
		b[i] = r_av;
		if(i + window == p->sample_count) break;
		r_av += a[i + window] - a[i];
	}

	int m = p->sample_count - window + 1;
	int step = p->sample_rate / 2;
	int j = 0;
	for(i = 0; i + step - 1 < m; i += step)
		a[j++] = vmax(b, i, i+step, NULL);
	qsort(a, j, sizeof(float), fl_cmp);
	float k = a[j/2];

	for(i = 0; i < p->sample_count; i++) {
		int j = i - window / 2;
		j = j < 0 ? 0 : j > p->sample_count - window ? p->sample_count - window : j;
		if(b[j] > 2*k) p->samples[i] = 0;
	}
}

void prepare_data(struct processing_buffers *b, int run_noise_suppressor)
{
	int i;

	memset(b->samples + b->sample_count, 0, b->sample_count * sizeof(float));
	run_filter(b->hpf, b->samples, b->sample_count);
#ifndef LIGHT
	if(run_noise_suppressor) noise_suppressor(b);
#endif

	for(i=0; i < b->sample_count; i++)
		b->samples[i] = fabs(b->samples[i]);

	run_filter(b->lpf, b->samples, b->sample_count);

	double average = 0;
	for(i=0; i < b->sample_count; i++)
		average += b->samples[i];
	average /= b->sample_count;
	for(i=0; i < b->sample_count; i++)
		b->samples[i] -= average;

	for(i=0; i < b->sample_rate/10; i++) {
		double k = ( 1 - cos(i*M_PI/(b->sample_rate/10)) ) / 2;
		b->samples[i] *= k;
		b->samples[b->sample_count - i - 1] *= k;
	}

	fftwf_execute(b->plan_a);
	for(i=0; i < b->sample_count+1; i++)
			b->sc_fft[i] = b->fft[i] * conj(b->fft[i]);
	fftwf_execute(b->plan_b);
}

int peak_detector(float *buff, int a, int b)
{
	int i_max;
	double max = vmax(buff, a, b+1, &i_max);
	if(max <= 0) return -1;

	int i;
	float v[b-a+1];
	for(i=a; i<=b; i++)
		v[i-a] = buff[i];
	qsort(v, b-a+1, sizeof(float), fl_cmp);
	float med = v[(b-a+1)/2];

	for(i=a+1; i<i_max; i++)
		if(buff[i] <= med) break;
	if(i==i_max) return -1;
	for(i=i_max+1; i<=b; i++)
		if(buff[i] <= med) break;
	if(i==b+1) return -1;

	int cnt = 0, down = 1;
	for(i=a+1; i<=b; i++) {
		if(buff[i] > (max + med) / 2) {
			cnt += down;
			down = 0;
		}
		if(buff[i] < med)
			down = 1;
	}
	debug("max = %f med = %f cnt = %d\n",max,med,cnt);
	if(cnt > 20)
		return -1;
	return i_max;
}

double estimate_period(struct processing_buffers *p)
{
	int first_estimate;
	vmax(p->samples_sc, p->sample_rate / 12, p->sample_rate, &first_estimate);
	first_estimate = peak_detector(p->samples_sc,
			fmax(p->sample_rate / 12, first_estimate - p->sample_rate / 12),
			first_estimate + p->sample_rate / 12);
	if(first_estimate == -1) {
		debug("no candidate period\n");
		return -1;
	}
	int estimate = first_estimate;
	int factor = 1;
	int fct;
	for(fct = 2; first_estimate / fct > p->sample_rate / 12; fct++) {
		int new_estimate = peak_detector(p->samples_sc,
					first_estimate / fct - p->sample_rate / 50,
					first_estimate / fct + p->sample_rate / 50);
		if(new_estimate > -1 && p->samples_sc[new_estimate] > 0.9 * p->samples_sc[first_estimate]) {
			estimate = new_estimate;
			factor = fct;
		}
	}
	int a = estimate*3/2 - p->sample_rate / 50;
	int b = estimate*3/2 + p->sample_rate / 50;
	double max = vmax(p->samples_sc, a, b+1, NULL);
	if(max < 0.2 * p->samples_sc[estimate]) {
		if(first_estimate * 2 / factor < p->sample_rate ) {
			debug("double triggered\n");
			return peak_detector(p->samples_sc,
					first_estimate * 2 / factor - p->sample_rate / 50,
					first_estimate * 2 / factor + p->sample_rate / 50);
		} else {
			debug("period rejected (immense beat error?)\n");
			return -1;
		}
	} else return estimate;
}

int compute_period(struct processing_buffers *b, int bph)
{
	double estimate;
	if(bph)
		estimate = peak_detector(b->samples_sc,
				7200 * b->sample_rate / bph - b->sample_rate / 50,
				7200 * b->sample_rate / bph + b->sample_rate / 50);
	else
		estimate = estimate_period(b);
	if(estimate == -1) {
		debug("failed to estimate period\n");
		return 1;
	}
	double delta = b->sample_rate * 0.02;
	double new_estimate = estimate;
	double sum = 0;
	double sq_sum = 0;
	int count = 0;
	int cycle = 1;
	for(;;) {
		int inf = floor(new_estimate * cycle - delta);
		int sup = ceil(new_estimate * cycle + delta);
		if(sup > b->sample_count * 2 / 3)
			break;
		new_estimate = peak_detector(b->samples_sc,inf,sup);
		if(new_estimate == -1) {
			debug("cycle = %d peak not found\n",cycle);
			return 1;
		}
		new_estimate /= cycle;
		if(new_estimate < estimate - delta || new_estimate > estimate + delta) {
			debug("cycle = %d new_estimate = %f invalid peak\n",cycle,new_estimate/b->sample_rate);
			return 1;
		} else
			debug("cycle = %d new_estimate = %f\n",cycle,new_estimate/b->sample_rate);
		if(inf > b->sample_count / 3) {
			sum += new_estimate;
			sq_sum += new_estimate * new_estimate;
			count++;
		}
		cycle++;
	}
	if(count > 0) estimate = sum / count;
	b->period = estimate;
	if(count > 1)
		b->sigma = sqrt((sq_sum - count * estimate * estimate)/ (count-1));
	else
		b->sigma = b->period;
	return 0;
}

float tmean(float *x, int n)
{
	if(n>16) {
		qsort(x,16,sizeof(float),fl_cmp);
		float t = x[12];
		int i, c1 = 0, c2 = 0;
		for(i = 0; i < n; i++) {
			if(x[i] < t) c1++;
			if(x[i] > t) c2++;
		}
		double r = (double)c1/(c1+c2);
		if(r < .9 && r > .7) {
			double sum = 0;
			for(i=0; i < n; i++)
				if(x[i] < t) sum += x[i];
			return sum/c1;
		}
	}
	qsort(x,n,sizeof(float),fl_cmp);
	int i;
	double sum = 0;
	for(i=0; i < n*4/5; i++)
		sum += x[i];
	return sum/(n*4/5);
}

void compute_phase(struct processing_buffers *p, double period)
{
	int i;
	double x = 0, y = 0;
	for(i = 0; i < period; i++) {
		int j;
		p->waveform[i] = 0;
		for(j=0;;j++) {
			int n = round(i + j * period);
			if(n >= p->sample_count) break;
			p->waveform[i] += p->samples[n];
		}
		p->waveform[i] /= j;
	}
	for(i=0; i<period; i++) {
		double a = i * 2 * M_PI / period;
		x += p->waveform[i] * cos(a);
		y += p->waveform[i] * sin(a);
	}
	p->phase = period * (M_PI + atan2(y,x)) / (2 * M_PI);
}

void compute_waveform(struct processing_buffers *p, int wf_size)
{
	int i;
	for(i=0; i<2*p->sample_rate; i++)
		p->waveform[i] = 0;
	for(i=0; i < wf_size; i++) {
		float bin[(int)ceil(1 + p->sample_count / wf_size)];
		int j;
		double k = fmod(i+p->phase,wf_size);
		for(j=0;;j++) {
			int n = round(k+j*wf_size);
			if(n >= p->sample_count) break;
			bin[j] = p->samples[n];
		}
		p->waveform[i] = tmean(bin,j);
	}

	int step = ceil(wf_size / 100);
	for(i=0; i * step < wf_size; i++)
		p->waveform_sc[i] = p->waveform[i * step];
	qsort(p->waveform_sc,i,sizeof(float),fl_cmp);
	double nl = p->waveform_sc[i/2];
	for(i=0; i<wf_size; i++)
		p->waveform[i] -= nl;

	p->waveform_max = vmax(p->waveform, 0, wf_size, NULL);
}

void prepare_waveform(struct processing_buffers *p)
{
	compute_phase(p,p->period/2);
	compute_waveform(p,ceil(p->period));

	int i;
	fftwf_execute(p->plan_c);
	for(i=0; i < p->sample_rate+1; i++)
			p->sc_fft[i] *= conj(p->sc_fft[i]);
	fftwf_execute(p->plan_d);
}

void prepare_waveform_cal(struct processing_buffers *p)
{
	compute_phase(p,p->sample_rate);
	compute_waveform(p,p->sample_rate);

#ifdef DEBUG
	memcpy(p->debug, p->waveform, p->sample_rate * sizeof(float));
#endif
}

void smooth(float *in, float *out, int window, int size)
{
	int i;
	double k = 1 - (1. / window);
	double r_av = 0;
	double u = 0;
	for(i=0; i < window; i++) {
		u *= k;
		float x = in[i];
		if(x > u) u = x;
		r_av += u;
	}
	double w = 0;
	for(i=0; i + window < size; i++) {
		out[i] = r_av;
		u *= k;
		w *= k;
		float x = in[i+window];
		float y = in[i];
		if(x > u) u = x;
		if(y > w) w = y;
		r_av += u - w;
	}
}

int compute_parameters(struct processing_buffers *p)
{
	int tic_to_toc = peak_detector(p->waveform_sc,
			floor(p->period/2)-p->sample_rate/50,
			floor(p->period/2)+p->sample_rate/50);
	if(tic_to_toc < 0) {
		debug("beat error = ---\n");
		return 1;
	} else {
		p->be = p->period/2 - tic_to_toc;
		debug("beat error = %.1f\n",fabs(p->be)*1000/p->sample_rate);
	}

	int wf_size = ceil(p->period);
	float fold_wf[wf_size - tic_to_toc];
	int i;
	for(i = 0; i < wf_size - tic_to_toc; i++)
		fold_wf[i] = p->waveform[i] + p->waveform[i+tic_to_toc];
	int window = p->sample_rate / 2000;
	float smooth_wf[wf_size - tic_to_toc - window];
	smooth(fold_wf, smooth_wf, window, wf_size - tic_to_toc);
	int max_i;
	float max = vmax(smooth_wf, 0, wf_size - tic_to_toc - window, &max_i);
	if(max <= 0) return 1;
	p->tic = max_i;
	p->toc = p->tic + tic_to_toc;

	double phase = p->timestamp - p->last_tic;
	double apparent_phase = p->sample_count - (p->phase + p->tic);
	double shift = fmod(apparent_phase - phase, p->period);
	if(shift < 0) shift += p->period;
	debug("shift = %.3f\n",shift / p->period);

	if(fabs(shift - p->period/2) < p->period/4) {
		p->last_toc = p->timestamp - (uint64_t)round(fmod(apparent_phase, p->period));
		int t = p->tic;
		p->tic = p->toc;
		p->toc = t;
		apparent_phase = p->sample_count - (p->phase + p->tic);
	} else
		p->last_toc = p->timestamp - (uint64_t)round(fmod(p->sample_count - (p->phase + p->toc), p->period));

	p->last_tic = p->timestamp - (uint64_t)round(fmod(apparent_phase, p->period));

	return 0;
}

void do_locate_events(int *events, struct processing_buffers *p, float *waveform, int last, int offset, int count)
{
	int i;
	memset(p->tic_wf, 0, p->sample_rate * sizeof(float));
	for(i=0; i<floor(p->period)/2; i++)
		p->tic_wf[i] = waveform[i];
	fftwf_execute(p->plan_e);

	int s;
	memset(p->tic_c, 0, 2 * p->sample_count * sizeof(float));
	for(s = p->sample_count - p->sample_rate/2; s >= 0; s -= p->sample_rate/2) {
		for(i=0; i < p->sample_rate; i++)
			p->slice_wf[i] = p->samples[i+s];
		fftwf_execute(p->plan_f);
		for(i=0; i < p->sample_rate/2+1; i++)
			p->slice_fft[i] *= conj(p->tic_fft[i]);
		fftwf_execute(p->plan_g);
		for(i=0; i < p->sample_rate/2; i++)
			p->tic_c[i+s] = p->slice_wf[i];
		if(s < last - offset - (count-1)*p->period - 0.02*p->sample_rate) break;
	}

	for(i=0; i<count; i++) {
		int a = round(last - offset - i*p->period - 0.02*p->sample_rate);
		int b = round(last - offset - i*p->period + 0.02*p->sample_rate);
		if(a < 0 || b >= p->sample_count - p->period/2)
			events[i] = -1;
		else {
			int peak = peak_detector(p->tic_c,a,b);
			events[i] = peak >= 0 ? offset + peak : -1;
		}
	}
}

void locate_events(struct processing_buffers *p)
{
	int count = 1 + ceil((p->timestamp - p->events_from) / p->period);
	if(count <= 0 || 2*count > EVENTS_MAX) {
		p->events[0] = 0;
		return;
	}

	int events[2*count];
	int half = p->tic < p->period/2 ? 0 : round(p->period / 2);
	int offset = p->tic - half;
	do_locate_events(events, p, p->waveform + half, (int)(p->last_tic + p->sample_count - p->timestamp), offset, count);
	half = p->toc < p->period/2 ? 0 : round(p->period / 2);
	offset = p->toc - half;
	do_locate_events(events+count, p, p->waveform + half, (int)(p->last_toc + p->sample_count - p->timestamp), offset, count);
	qsort(events, 2*count, sizeof(int), int_cmp);

	int i,j;
	for(i=0, j=0; i < 2*count; i++) {
		if(events[i] < 0 ||
				events[i] + p->timestamp < p->sample_count ||
				events[i] + p->timestamp - p->sample_count < p->events_from)
			continue;
		p->events[j++] = events[i] + p->timestamp - p->sample_count;
	}
	p->events[j] = 0;
}

void compute_amplitude(struct processing_buffers *p)
{
	int i,j,k;

	int wf_size = ceil(p->period);
	int window = p->sample_rate / 1000;
	for(i = 0; i < window; i++)
		p->waveform[i + wf_size] = p->waveform[i];
	float smooth_wf[wf_size];
	smooth(p->waveform, smooth_wf, window, wf_size + window);

	double max = 0;
	for(k = 0; k < 2; k++) {
		j = floor(fmod((k ? p->tic : p->toc) + p->period/8, p->period));
		for(i = 0; i < p->period/8; i++) {
			if(smooth_wf[j] > max) max = smooth_wf[j];
			if(++j > p->period) j = 0;
		}
	}
	double glob_max = vmax(smooth_wf, 0, ceil(p->period), NULL);
	double threshold = fmax(.01 * glob_max, 1.4 * max);
	debug("amp threshold from %s\n", .01 * glob_max > 1.2 * max ? "global maximum" : "noise level");
	for(k = 0; k < 2; k++) {
		double max = 0;
		j = floor(fmod((k ? p->tic : p->toc) - p->period/8, p->period));
		for(i = 0; i < p->period/8; i++) {
			if(smooth_wf[j] > threshold) break;
			if(++j > p->period) j = 0;
		}
		for(; i < p->period/8; i++) {
			double x = smooth_wf[j];
			if(x > max) max = x;
			else break;
			if(++j > p->period) j = 0;
		}
		double pulse = i < p->period/8 ? p->period/8 - i - 1: -1;
		if(k)
			p->tic_pulse = pulse;
		else
			p->toc_pulse = pulse;
		debug("amp %s pulse = %f\n", k ? "tic" : "toc", 1000 * pulse / p->sample_rate);
	}
}

void compute_cal(struct calibration_data *cd, int sample_rate)
{
	int i;
	double x = 0, y = 0;
	for(i=0;i<cd->size;i++) {
		x += cos(cd->phases[i] * 2 * M_PI);
		y += sin(cd->phases[i] * 2 * M_PI);
	}
	double c = (atan2(y,x) - M_PI) / (2 * M_PI);
	double x_av = 0, y_av = 0;
	for(i=0;i<cd->size;i++) {
		x_av += cd->times[i];
		y_av += cd->phases[i] = fmod(cd->phases[i] - c, 1);
	}
	x_av /= cd->size;
	y_av /= cd->size;
	double n = 0, d = 0;
	for(i=0;i<cd->size;i++) {
		double x = cd->times[i] - x_av;
		n += x * (cd->phases[i] - y_av);
		d += x * x;
	}
	cd->calibration = n * 3600 * 24 / d;
	cd->state = 1;
	debug("Calibration result: %f s/d\n",cd->calibration);
}

void process(struct processing_buffers *p, int bph)
{
	prepare_data(p,1);
	p->ready = !compute_period(p,bph);
	if(p->period >= p->sample_rate / 2) {
		debug("Detected period too long\n");
		p->ready = 0;
	}
	if(!p->ready) {
		debug("abort after compute_period()\n");
		return;
	}
	prepare_waveform(p);
	p->ready = !compute_parameters(p);
	if(!p->ready) {
		debug("abort after compute_parameters()\n");
		return;
	}
	locate_events(p);
	compute_amplitude(p);
}

int test_cal(struct processing_buffers *p)
{
	prepare_data(p,0);
	return compute_period(p,7200);
}

int process_cal(struct processing_buffers *p, struct calibration_data *cd)
{
	prepare_data(p,0);
	if(compute_period(p,7200)) {
		debug("abort after compute_period()\n");
		return 1;
	}
	prepare_waveform_cal(p);
	int i;
	double phase = -1;
	for(i = p->sample_rate/4; i < p->sample_rate/2; i++)
		if(p->waveform[i] > p->waveform_max/3)
			phase = fmod((p->timestamp + p->phase + i) / p->sample_rate, 1.);
	if(phase < 0) {
		debug("unable to find rising edge\n");
		return 1;
	}
	debug("Phase = %f\n",phase);
	if(cd->wp < cd->size) {
		if(cd->wp == 0)
			cd->start_time = p->timestamp;
		double time = (double)(p->timestamp - cd->start_time) / p->sample_rate;
		if(cd->wp == 0 || time > cd->times[cd->wp-1] + 1) {
			cd->times[cd->wp] = time;
			cd->phases[cd->wp] = phase;
			cd->wp++;
		}
	}
	if(cd->wp == cd->size && cd->state == 0)
		compute_cal(cd, p->sample_rate);
	return 0;
}
