#include "tg.h"

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
	b->tic_wf = fftwf_malloc(2 * b->sample_count * sizeof(float));
	b->tic_c = fftwf_malloc(2 * b->sample_count * sizeof(float));
	b->tic_fft = fftwf_malloc((b->sample_count + 1) * sizeof(fftwf_complex));
	b->plan_a = fftwf_plan_dft_r2c_1d(2 * b->sample_count, b->samples, b->fft, FFTW_ESTIMATE);
	b->plan_b = fftwf_plan_dft_c2r_1d(2 * b->sample_count, b->sc_fft, b->samples_sc, FFTW_ESTIMATE);
	b->plan_c = fftwf_plan_dft_r2c_1d(2 * b->sample_rate, b->waveform, b->sc_fft, FFTW_ESTIMATE);
	b->plan_d = fftwf_plan_dft_c2r_1d(2 * b->sample_rate, b->sc_fft, b->waveform_sc, FFTW_ESTIMATE);
	b->plan_e = fftwf_plan_dft_r2c_1d(2 * b->sample_count, b->tic_wf, b->tic_fft, FFTW_ESTIMATE);
	b->plan_f = fftwf_plan_dft_c2r_1d(2 * b->sample_count, b->sc_fft, b->tic_c, FFTW_ESTIMATE);
	b->hpf = malloc(sizeof(struct filter));
	make_hp(b->hpf,(double)FILTER_CUTOFF/b->sample_rate);
	b->lpf = malloc(sizeof(struct filter));
	make_lp(b->lpf,(double)FILTER_CUTOFF/b->sample_rate);
	b->events = malloc(EVENTS_MAX * sizeof(uint64_t));
	b->ready = 0;
}

struct processing_buffers *pb_clone(struct processing_buffers *p)
{
	struct processing_buffers *new = malloc(sizeof(struct processing_buffers));
	new->sample_rate = p->sample_rate;
	new->waveform = malloc(new->sample_rate * sizeof(float));
	memcpy(new->waveform, p->waveform, new->sample_rate * sizeof(float));

	new->sample_count = p->sample_count;
	new->samples_sc = malloc(new->sample_count * sizeof(float));
	memcpy(new->samples_sc, p->samples_sc, new->sample_rate * sizeof(float));

	new->period = p->period;
	new->sigma = p->sigma;
	new->be = p->be;
	new->waveform_max = p->waveform_max;
	new->tic = p->tic;
	new->toc = p->toc;
	new->ready = p->ready;
	return new;
}

void pb_destroy(struct processing_buffers *p)
{
	// Future BUG: we free only waveform
	free(p->waveform);
	free(p->samples_sc);
	free(p);
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
	float max = 0;
	int j = 0;
	for(i = 0; i < m; i++) {
		if(b[i] > max) max = b[i];
		if((i+1) % (p->sample_rate/2) == 0) {
			a[j++] = max;
			max = 0;
		}
	}
	qsort(a, j, sizeof(float), fl_cmp);
	float k = a[j/2];

	for(i = 0; i < p->sample_count; i++) {
		int j = i - window / 2;
		j = j < 0 ? 0 : j > p->sample_count - window ? p->sample_count - window : j;
		if(b[j] > 2*k) p->samples[i] = 0;
	}
}

void prepare_data(struct processing_buffers *b)
{
	int i;
	int first_fft_size = b->sample_count/2 + 1;

	memset(b->samples + b->sample_count, 0, b->sample_count * sizeof(float));
	run_filter(b->hpf, b->samples, b->sample_count);
	noise_suppressor(b);

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
	int i;
	double max = buff[a];
	int i_max = a;
	for(i=a+1; i<=b; i++) {
		if(buff[i] > max) {
			max = buff[i];
			i_max = i;
		}
	}
	if(max <= 0) return -1;
	//return i_max;
	float v[b-a+1];
	for(i=a; i<=b; i++)
		v[i-a] = buff[i];
	qsort(v, b-a+1, sizeof(float), fl_cmp);
	float x = v[(b-a+1)/10];
	float y = v[(b-a+1)/2];
	debug("max = %f  x = %f  y = %f\n",max,x,y);
	if(max < 4*y - 3*x)
		return -1;
	return i_max;
}

double estimate_period(struct processing_buffers *p)
{
	int estimate = peak_detector(p->samples_sc, p->sample_rate / 12, p->sample_rate );
	if(estimate == -1) {
		debug("no candidate period\n");
		return -1;
	}
	int a = estimate*3/2 - p->sample_rate / 50;
	int b = estimate*3/2 + p->sample_rate / 50;
	double max = p->samples_sc[a];
	int i;
	for(i=a+1; i<=b; i++)
		if(p->samples_sc[i] > max)
			max = p->samples_sc[i];
	if(max < 0.2 * p->samples_sc[estimate]) {
		if(estimate * 2 < p->sample_rate ) {
			debug("double triggered\n");
			return peak_detector(p->samples_sc,
					estimate*2 - p->sample_rate / 50,
					estimate*2 + p->sample_rate / 50);
		} else {
			debug("period rejected (immense beat error?)");
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
	if(estimate >= b->sample_rate / 2) return 1;
	b->period = estimate;
	if(count > 1)
		b->sigma = sqrt((sq_sum - count * estimate * estimate)/ (count-1));
	else
		b->sigma = b->period;
	return 0;
}

float tmean(float *x, int n)
{
	qsort(x,n,sizeof(float),fl_cmp);
	int i;
	double sum = 0;
	for(i=0; i < n*4/5; i++)
		sum += x[i];
	return sum/(n*4/5);
}

int compute_parameters(struct processing_buffers *p)
{
	int i;
	double x = 0, y = 0;
	for(i=0; i<p->sample_count; i++) {
		double a = i * 4 * M_PI / p->period;
		x += p->samples[i] * cos(a);
		y += p->samples[i] * sin(a);
	}
	double s = p->period * (M_PI + atan2(y,x)) / (4 * M_PI);

	for(i=0; i<2*p->sample_rate; i++)
		p->waveform[i] = 0;

	float bin[(int)ceil(1 + p->sample_count / p->period)];
	for(i=0; i < p->period; i++) {
		int j;
		double k = fmod(i+s,p->period);
		for(j=0;;j++) {
			int n = round(k+j*p->period);
			if(n >= p->sample_count) break;
			bin[j] = p->samples[n];
		}
		p->waveform[i] = tmean(bin,j);
	}

	for(i=0; i<p->period; i++)
		p->waveform_sc[i] = p->waveform[i];
	qsort(p->waveform_sc,floor(p->period),sizeof(float),fl_cmp);
	double nl = p->waveform_sc[(int)floor(p->period/2)];
	for(i=0; i<p->period; i++) {
		p->waveform[i] -= nl;
		//p->waveform[i] = cbrt(p->waveform[i]);
		//if(p->waveform[i] <= 0) p->waveform[i] = 0;
	}

	fftwf_execute(p->plan_c);
	for(i=0; i < p->sample_rate+1; i++)
			p->sc_fft[i] = p->sc_fft[i] * conj(p->sc_fft[i]);
	fftwf_execute(p->plan_d);

//	for(i=0;i<p->period;i++)
//		p->samples_sc[i] = p->waveform_sc[i];

	int tic_to_toc = peak_detector(p->waveform_sc,
			floor(p->period/2)-p->sample_rate/50,
			floor(p->period/2)+p->sample_rate/50);
	if(tic_to_toc < 0) {
		p->tic = p->toc = -1;
		p->be = -1;
		debug("beat error = ---\n");
		return 1;
	} else {
		p->be = p->period/2 - tic_to_toc;
		debug("beat error = %.1f\n",fabs(p->be)*1000/p->sample_rate);
	}

	double max = 0;
	int max_i = -1;
	for(i=0;i<p->period;i++) {
		if(p->waveform[i] > max) {
			max = p->waveform[i];
			max_i = i;
		}
	}
	p->waveform_max = max;

	if(max_i < p->period/2) {
		p->tic = max_i;
		p->toc = max_i + tic_to_toc;
		if(p->toc > p->period)
			p->toc = floor(p->period);
	} else {
		p->toc = max_i;
		p->tic = max_i - tic_to_toc;
		if(p->tic < 0)
			p->tic = 0;
	}

	double phase = p->timestamp - p->last_tic;
	double apparent_phase = p->sample_count - (s + p->tic);
	double shift = fmod(apparent_phase - phase, p->period);
	if(shift < 0) shift += p->period;
	debug("shift = %.3f\n",shift / p->period);

	if(fabs(shift - p->period/2) < p->period/4) {
		p->last_toc = p->timestamp - (uint64_t)round(fmod(apparent_phase, p->period));
		int t = p->tic;
		p->tic = p->toc;
		p->toc = t;
		apparent_phase = p->sample_count - (s + p->tic);
	} else
		p->last_toc = p->timestamp - (uint64_t)round(fmod(p->sample_count - (s + p->toc), p->period));

	p->last_tic = p->timestamp - (uint64_t)round(fmod(apparent_phase, p->period));

	return 0;
}

void do_locate_events(int *events, struct processing_buffers *p, float *waveform, int last, int offset, int count)
{
	int i;
	memset(p->tic_wf, 0, 2 * p->sample_count * sizeof(float));
	for(i=0; i<floor(p->period)/2; i++)
		p->tic_wf[i] = waveform[i];

	fftwf_execute(p->plan_e);
	for(i=0; i < p->sample_count+1; i++)
			p->sc_fft[i] = p->fft[i] * conj(p->tic_fft[i]);
	fftwf_execute(p->plan_f);

	for(i=0; i<count; i++) {
		int a = round(last - offset - i*p->period - 0.02*p->sample_rate);
		int b = round(last - offset - i*p->period + 0.02*p->sample_rate);
		if(a < 0 || b >= p->sample_count)
			events[i] = -1;
		else
			events[i] = offset + peak_detector(p->tic_c,a,b);
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
	do_locate_events(events, p, p->waveform + half, p->last_tic + p->sample_count - p->timestamp, offset, count);
	half = p->toc < p->period/2 ? 0 : round(p->period / 2);
	offset = p->toc - half;
	do_locate_events(events+count, p, p->waveform + half, p->last_toc + p->sample_count - p->timestamp, offset, count);
	qsort(events, 2*count, sizeof(int), int_cmp);

	int i,j;
	for(i=0, j=0; i < 2*count; i++) {
		if(events[i] < 0 || events[i] + p->timestamp - p->sample_count < p->events_from)
			continue;
		p->events[j++] = events[i] + p->timestamp - p->sample_count;
	}
	p->events[j] = 0;
/*
	int i;
	debug("events = ");
	for(i=0; i<2*count; i++)
		debug("%d ", events[i]);
	debug("\n");
*/
	for(i=0;i<p->sample_count;i++)
		p->samples_sc[i] = p->tic_c[i];
}

void process(struct processing_buffers *p, int bph)
{
	int i;
	prepare_data(p);
	p->ready = ! ( compute_period(p,bph) || compute_parameters(p) );
	if(p->ready) locate_events(p);
}
