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

static int int_cmp(const void *a, const void *b)
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

static void make_lp(struct filter *f, double freq)
{
	double K = tan(M_PI * freq);
	double norm = 1 / (1 + K * sqrt(2) + K * K);
	f->a0 = K * K * norm;
	f->a1 = 2 * f->a0;
	f->a2 = f->a0;
	f->b1 = 2 * (K * K - 1) * norm;
	f->b2 = (1 - K * sqrt(2) + K * K) * norm;
}

static void run_filter(struct filter *f, float *buff, int size)
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
	b->lpf = malloc(sizeof(struct filter));
	make_lp(b->lpf,(double)FILTER_CUTOFF/b->sample_rate);
	b->events = malloc(EVENTS_MAX * sizeof(uint64_t));
	b->ready = 0;
#ifdef DEBUG
	b->debug_size = b->sample_count;
	b->debug = fftwf_malloc(b->debug_size * sizeof(float));
#endif
}

void pb_destroy(struct processing_buffers *b)
{
	fftwf_free(b->samples);
	free(b->samples_sc);
	free(b->waveform);
	free(b->waveform_sc);
	fftwf_free(b->fft);
	fftwf_free(b->sc_fft);
	fftwf_free(b->tic_wf);
	fftwf_free(b->slice_wf);
	fftwf_free(b->tic_fft);
	fftwf_free(b->slice_fft);
	free(b->tic_c);
	fftwf_destroy_plan(b->plan_a);
	fftwf_destroy_plan(b->plan_b);
	fftwf_destroy_plan(b->plan_c);
	fftwf_destroy_plan(b->plan_d);
	fftwf_destroy_plan(b->plan_e);
	fftwf_destroy_plan(b->plan_f);
	fftwf_destroy_plan(b->plan_g);
	free(b->lpf);
	free(b->events);
#ifdef DEBUG
	fftwf_free(b->debug);
#endif
}

struct processing_buffers *pb_clone(struct processing_buffers *p)
{
	struct processing_buffers *new = malloc(sizeof(struct processing_buffers));
	new->sample_count = ceil(p->period);
	new->waveform = malloc(new->sample_count * sizeof(float));
	memcpy(new->waveform, p->waveform, new->sample_count * sizeof(float));
	if(p->events) {
		new->events = malloc(EVENTS_MAX * sizeof(uint64_t));
		memcpy(new->events, p->events, EVENTS_MAX * sizeof(uint64_t));
	} else
		new->events = NULL;

#ifdef DEBUG
	new->debug_size = p->debug_size;
	if(p->debug) {
		new->debug = malloc(new->debug_size * sizeof(float));
		memcpy(new->debug, p->debug, new->debug_size * sizeof(float));
	} else
		new->debug = NULL;
#endif

	new->sample_rate = p->sample_rate;
	new->period = p->period;
	new->sigma = p->sigma;
	new->be = p->be;
	new->waveform_max = p->waveform_max;
	new->tic_pulse = p->tic_pulse;
	new->toc_pulse = p->toc_pulse;
	new->amp = p->amp;
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

static float vmax(float *v, int a, int b, int *i_max)
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

/* Choose pivot: use median of first, last, middle elements */
static int pivot(const float *x, int l, int r)
{
	const int m = (l+r)/2;
	if (x[l] < x[r]) {
		if (x[m] <= x[l])
			return l;
		if (x[m] >= x[r])
			return r;
		return m;
	} else {
		if (x[m] <= x[r])
			return r;
		if (x[m] >= x[l])
			return l;
		return m;
	}
}

/** Partition list in ascending order at rank k.
 *
 * Re-orders the list so that the k'th largest values are in the first k
 * elements, the (k+1)'th largest value is x[k], and all the values smaller than
 * x[k] follow it.  Thus, x[0 .. k-1] >= x[k] >= x[k+1 .. n-1].  Note that
 * x[0 .. k-1] and x[k+1 .. n-1] are not sorted, so this is like, but not the
 * same as, a quicksort.  If we avoid worst case behavior in choice of pivots,
 * it's O(n) rather than O(n log n) like quicksort.
 *
 * This uses the "median of three" pivot strategy.  I also tried simpler ones,
 * like always use the last element, but this benchmarked faster.
 *
 * @param[in,out] x The values to partition.
 * @param[in] n The number of values.
 * @param[in] k The index to partition at.
 * @returns Nothing, but x will be partitioned.
 */
static void quickselect(float* x, int n, int k)
{
	int l = 0, r = n - 1;
	while (1) {
		if (l == r)
			return;
		if (r - l == 1) {
			/* Only two values left, put them in decending order */
			if (x[l] < x[r]) {
				float t = x[r];
				x[r] = x[l];
				x[l] = t;
			}
			return;
		}

		int p = pivot(x, l, r);
		const float pv = x[p];
		x[p] = x[r];
		p = l;
		/* Partition:  Everything greater than pv at start, then pv at
		 * position p, then everything smaller.  */
		int i;
		for (i = l; i < r; i++) {
			if (x[i] > pv) {
				float t = x[i];
				x[i] = x[p];
				x[p++] = t;
			}
		}
		/* Swap pivot value into position p */
		x[r] = x[p];
		x[p] = pv;
		if (k == p)
			return;
		if (k < p)
			r = p - 1;
		else
			l = p + 1;
	};
}

static void noise_suppressor(struct processing_buffers *p)
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
	quickselect(a, j, j/2);
	float k = a[j/2];

	for(i = 0; i < p->sample_count; i++) {
		int j = i - window / 2;
		j = j < 0 ? 0 : j > p->sample_count - window ? p->sample_count - window : j;
		if(b[j] > 2*k) p->samples[i] = 0;
	}
}

static void prepare_data(struct processing_buffers *b, int run_noise_suppressor)
{
	int i;

	memset(b->samples + b->sample_count, 0, b->sample_count * sizeof(float));
	if(run_noise_suppressor) noise_suppressor(b);

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

#ifdef DEBUG
	memcpy(b->debug, b->samples_sc, b->sample_count * sizeof(float));
#endif
}

static int peak_detector(float *buff, int a, int b)
{
	int i_max;
	double max = vmax(buff, a, b+1, &i_max);
	if(max <= 0) return -1;

	int i;
	float v[b-a+1];
	memcpy(v, buff + a, sizeof(v));
	quickselect(v, b-a+1, (b-a+1)/2);
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

static double estimate_period(struct processing_buffers *p)
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

static int compute_period(struct processing_buffers *b, int bph)
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

/** Return the mean with the greatest value excluded.
 *
 * Finds the mean of the values in x with the greatest value excluded from the
 * list.  If n <= 5, then this is the same as a the mean of the lower four
 * quintiles, since the upper quintile is 1 value.
 *
 * @param[in] x The values.
 * @param[in] n Number of values.
 * @returns The mean with the greatest value excluded.
 */
static float mean_less_greatest(const float *x, int n)
{
	float greatest = x[0], sum = x[0];
	int i;
	for(i = 1; i < n; i++) {
		sum += x[i];
		if (x[i] > greatest) greatest = x[i];
	}
	return (sum - greatest) / (n-1);
}

/** Return the mean with the greatest two values excluded.
 *
 * Finds the mean of the values in x with the greatest and 2nd greatest values
 * excluded from the list.  If 5 < n <= 10, then this is the same as a the mean
 * of the lower four quintiles, since the upper quintile is 2 values.
 *
 * @param[in] x The values.
 * @param[in] n Number of values.
 * @returns The mean with the greatest two values excluded.
 */
static float mean_less_two_greatest(const float *x, int n)
{
	float sum = x[0], greatest[2] = {x[0], FLT_MIN};
	int i;
	for(i = 1; i < n; i++) {
		sum += x[i];
		if (x[i] > greatest[0]) {
			greatest[1] = greatest[0];
			greatest[0] = x[i];
		} else if (x[i] > greatest[1]) {
			greatest[1] = x[i];
		}
	}
	return (sum - greatest[0] - greatest[1]) / (n-2);
}

/** Find the mean of the lower four quintiles.
 *
 * Find mean of the lower four quintiles of x, i.e. the mean of the values in x
 * with the largest 20% excluded.
 *
 * @warning This may modify the list to re-order the values.
 *
 * @param[in] x The values.
 * @param[n] n The number of values.
 * @returns The mean of the lower four quintiles.
 */
static float tmean(float *x, int n)
{
	/* Use a hybrid approach, where for small lists a specialized function
	 * is used, since only a few items will be looked at and just one or two
	 * excluded.  So the simple and linear processing is faster.  For larger
	 * lists, use a quickselect based algorithm with average case O(n)
	 * performance rather than the O(n^2) that the first algorithm has.
	 *
	 * After size 10, the specialized functions don't provide much benefit.
	 * */
	if (n <= 5)
		return mean_less_greatest(x, n);
	else if (n <= 10)
		return mean_less_two_greatest(x, n);
	else {
		int k = (n+4)/5;	/* Index of upper quintile */
		quickselect(x, n, k);
		/* Now x[0] to x[k-1] should be the values to exclude */

		double sum = 0;
		int i;
		for(i = k; i < n; i++) sum += x[i];
		return sum / (n-k);
	}
}

static void compute_phase(struct processing_buffers *p, double period)
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

static void compute_waveform(struct processing_buffers *p, int wf_size)
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
	quickselect(p->waveform_sc, i, i/2);
	double nl = p->waveform_sc[i/2];
	for(i=0; i<wf_size; i++)
		p->waveform[i] -= nl;

	p->waveform_max = vmax(p->waveform, 0, wf_size, &p->waveform_max_i);
}

static void prepare_waveform(struct processing_buffers *p)
{
	compute_phase(p,p->period/2);
	compute_waveform(p,ceil(p->period));

	int i;
	fftwf_execute(p->plan_c);
	for(i=0; i < p->sample_rate+1; i++)
			p->sc_fft[i] *= conj(p->sc_fft[i]);
	fftwf_execute(p->plan_d);
}

static void prepare_waveform_cal(struct processing_buffers *p)
{
	compute_phase(p,p->sample_rate);
	compute_waveform(p,p->sample_rate);
}

static void smooth(float *in, float *out, int window, int size)
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

static int compute_parameters(struct processing_buffers *p)
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

static void do_locate_events(int *events, struct processing_buffers *p, float *waveform, int last, int offset, int count)
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

static void locate_events(struct processing_buffers *p)
{
	int count = 1 + ceil((p->timestamp - p->events_from) / p->period);
	if(count <= 0 || 2*count >= EVENTS_MAX) {
		p->events[0] = 0;
		return;
	}

	int events[2*count];
	int half = p->tic < p->period/2 ? 0 : round(p->period / 2);
	int offset = p->tic - half - (p->tic_pulse - p->toc_pulse) / 2;
	do_locate_events(events, p, p->waveform + half, (int)(p->last_tic + p->sample_count - p->timestamp), offset, count);
	half = p->toc < p->period/2 ? 0 : round(p->period / 2);
	offset = p->toc - half - (p->toc_pulse - p->tic_pulse) / 2;
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

static void compute_amplitude(struct processing_buffers *p, double la)
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
	debug("amp threshold from %s\n", .01 * glob_max > 1.4 * max ? "global maximum" : "noise level");

	p->amp = -1;
	p->tic_pulse = p->toc_pulse = -1;
	while(threshold < .2 * glob_max) {
		debug("amp threshold = %f%% glob max\n", threshold * 100 / glob_max);
		double tic_pulse = -1;
		double toc_pulse = -1;
		for(k = 0; k < 2; k++) {
			double max = 0;
			j = floor(fmod((k ? p->tic : p->toc) + 7*p->period/8, p->period));
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
			if(i < p->period/8) {
				double pulse = p->period/8 - i - 1;
				if(k) tic_pulse = pulse;
				else toc_pulse = pulse;
				debug("amp %s pulse = %f\n", k ? "tic" : "toc", 1000 * pulse / p->sample_rate);
			} else
				goto next_threshold;
		}
		double tic_amp_abs = .5 / sin(M_PI * tic_pulse / p->period);
		double toc_amp_abs = .5 / sin(M_PI * toc_pulse / p->period);
		double tic_amp = la * tic_amp_abs;
		double toc_amp = la * toc_amp_abs;
		if(135 < tic_amp && tic_amp < 360 && 135 < toc_amp && toc_amp < 360 && fabs(tic_amp - toc_amp) < 60) {
			p->amp = (tic_amp_abs + toc_amp_abs) / 2;
			p->tic_pulse = tic_pulse;
			p->toc_pulse = toc_pulse;
			p->be = p->period/2 - fabs(p->toc - p->tic + p->tic_pulse - p->toc_pulse);
			debug("amp: be = %.1f\n",fabs(p->be)*1000/p->sample_rate);
			debug("amp = %f\n", la * p->amp);
			break;
		} else
			debug("amp rejected\n");
next_threshold:	threshold *= 1.4;
	}
	if(p->amp < 0) debug("amp failed\n");
}

void setup_cal_data(struct calibration_data *cd)
{
	cd->size = CAL_DATA_SIZE;
	cd->times = malloc(cd->size * sizeof(double));
	cd->phases = malloc(cd->size * sizeof(double));
	cd->events = malloc(cd->size * sizeof(uint64_t));
}

void cal_data_destroy(struct calibration_data *cd)
{
	free(cd->times);
	free(cd->phases);
	free(cd->events);
}

static int add_sample_cal(struct processing_buffers *p, struct calibration_data *cd)
{
	int i;
	double phase = -1;
	if(p->waveform_max_i < p->sample_rate*4/10 || p->waveform_max_i > p->sample_rate*6/10) {
		debug("unable to lock on signal\n");
		return 1;
	}
	double maxa = vmax(p->waveform,0,p->sample_rate/4,NULL);
	double maxb = vmax(p->waveform,p->sample_rate*3/4,p->sample_rate,NULL);
	double max = fmax(maxa,maxb);
	double thd = max + (p->waveform_max - max) * 0.05;
	for(i = p->sample_rate/4; i <= p->waveform_max_i; i++) {
		if(p->waveform[i] > thd) {
			if(i > p->sample_rate*4/10)
				phase = fmod((p->timestamp + p->phase + i) / p->sample_rate, 1.);
			break;
		}
	}
	if(phase < 0) {
		debug("unable to find rising edge\n");
		return 1;
	}
	debug("Phase = %f\n",phase);
	if(cd->wp < cd->size) {
		if(cd->wp == 0)
			cd->start_time = p->timestamp;
		double time = (double)(p->timestamp - cd->start_time) / p->sample_rate;
		if(cd->wp == 0 || time > cd->times[cd->wp-1] + 0.9) {
			cd->times[cd->wp] = time;
			cd->phases[cd->wp] = phase;
			cd->events[cd->wp] = (p->timestamp - p->timestamp % p->sample_rate) +
						(uint64_t)floor(phase * p->sample_rate);
			cd->wp++;
		}
	}
	return 0;
}

static void compute_cal(struct calibration_data *cd)
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
	double xx = 0, xy = 0, yy = 0;
	for(i=0;i<cd->size;i++) {
		double x = cd->times[i] - x_av;
		double y = cd->phases[i] - y_av;
		xx += x * x;
		xy += x * y;
		yy += y * y;
	}
	cd->calibration = xy * 3600 * 24 / xx;
	double delta = sqrt((xx * yy - xy * xy) / (cd->size - 2)) / xx;
	debug("Calibration result: %f s/d +- %f\n",cd->calibration,delta*3600*24);
	cd->state = delta * 3600 * 24 < 0.1 ? 1 : -1;
}

void process(struct processing_buffers *p, int bph, double la, int light)
{
	prepare_data(p, !light);
	p->ready = !compute_period(p,bph);
	if(p->ready && p->period >= p->sample_rate / 2) {
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
	compute_amplitude(p, la);
	locate_events(p);
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
	if(add_sample_cal(p,cd))
		return 1;
	if(cd->wp == cd->size && cd->state == 0)
		compute_cal(cd);
	return 0;
}
