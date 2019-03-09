#include "WaveEdit.hpp"
#include <string.h>


/*
double Oscillator::trap3() const {
    double pulseWidth = std::fmin(0.9999, this->pulseWidth);
    double scale = 1 / (1 - pulseWidth);

    double y = 4 * t;
    if (y >= 3) {
        y -= 4;
    } else if (y >= 1) {
        y = 2 - y;
    }
    y = std::fmax(-1, std::fmin(1, scale * y));

    double t1 = t + 0.25 - 0.25 * pulseWidth;
    t1 -= bitwiseOrZero(t1);

    double t2 = t1 + 0.5;
    t2 -= bitwiseOrZero(t2);

    // Triangle #1
    y += scale * 2 * freqInSecondsPerSample * (blamp(t1, freqInSecondsPerSample) - blamp(t2, freqInSecondsPerSample));

    t1 = t + 0.25 + 0.25 * pulseWidth;
    t1 -= bitwiseOrZero(t1);

    t2 = t1 + 0.5;
    t2 -= bitwiseOrZero(t2);

    // Triangle #2
    y += scale * 2 * freqInSecondsPerSample * (blamp(t1, freqInSecondsPerSample) - blamp(t2, freqInSecondsPerSample));

    return amplitude * y;
}
*/

Oscillator osc_a, osc_b;

/*

const char *waveShapeNames[WAVE_SHAPES_LEN] {
	"Empty",
	"Double Sine",
	"Multi Sine",
	"Triangle",
	"Double Triangle",
	"Saw",
	"Double Saw",
	"Trapezoid",
	"Square",
	"Double Square",
	"Pulse",
	"Double Pulse",
};
*/


void BaseWave::clear() {
	memset(this, 0, sizeof(BaseWave));
	lower_shape = Oscillator::SINE;
	upper_shape = Oscillator::SINE;
	pulse_width = 0.5;
	lock_shapes = true;
	brightness = 0.0;
	bottom_angle = 0.0;
	bottom_magnitude = 0.0;
	bottom_x = 0.0;
	bottom_y = 0.0;
	top_angle = 0.0;
	top_magnitude = 0.0;
	top_x = 0.0;
	top_y = 0.0;
	bezier_ratio = 0.0;
	bezier_weight = 0.0;
	updateShape();
	updatePhasor();
	generateSamples();
}


void BaseWave::updateShape() {	
	float linear_phasor[WAVE_LEN];
	for (int i = 0; i < WAVE_LEN; i++)
		linear_phasor[i] = (float) i / WAVE_LEN;
	generateShape(linear_phasor, shape);
};

void BaseWave::generateShape(const float *shape_phasor, float *samples) {
	// Generate base wave shape using lower/upper wave shapes and brightness
	float flshape = lower_shape * (WAVE_SHAPES_LEN - 1);
	float fushape = upper_shape * (WAVE_SHAPES_LEN - 1); 
	float flshape1 = floor(flshape);
	float fushape1 = floor(fushape);
	float flshape2 = fmod(flshape1 + 1.0, WAVE_SHAPES_LEN);
	float fushape2 = fmod(fushape1 + 1.0, WAVE_SHAPES_LEN);
	flshape = fmod(flshape, 1.0);
	fushape = fmod(fushape, 1.0);
	float phase;
	//osc_a.sync(0.0);
	//osc_b.sync(0.0);
	
	for (int i = 0; i < WAVE_LEN; i++) {
		phase = shape_phasor[i];
		if (i == WAVE_LEN) phase = 0.0;
		float fshape, fshape1, fshape2;
		//if (i < WAVE_LEN / 2) {
		if (phase < 0.5) {
			fshape1 = flshape1;
			fshape2 = flshape2;
			fshape = flshape;
		}
		else {
			fshape1 = fushape1;
			fshape2 = fushape2;
			fshape = fushape;
		};
		fshape1 = getShape(&osc_a, (Oscillator::Waveform) (int) fshape1, phase);
		fshape2 = getShape(&osc_b, (Oscillator::Waveform) (int) fshape2, phase);
		
		samples[i] = crossf(fshape1, fshape2, fshape);
	};
};


float BaseWave::getShape(Oscillator *osc, Oscillator::Waveform waveform, float phase) {
	osc->setSampleRate(rescalef(brightness, 0.0, 1.0, WAVE_LEN / 4, WAVE_LEN));
	osc->setPulseWidth(clampd(pulse_width, 0.0, 1.0));
	osc->setWaveform(waveform);
	
	if (pulse_width < 0.5) {
		phase += (0.25 - pulse_width / 2);
	}
	else if (pulse_width > 0.75) {
		phase -= (0.75 - pulse_width);
	}
		
	osc->sync(fmod(phase + 0.75, 1.0));
	return osc->getAndInc();
		
	/*
	int n;
	switch (shape) {
		case EMPTY:
			return -1.0f;
		case DOUBLE_SINE:
			return sin(-0.5 * M_PI + 4 * M_PI * phase);
		case MULTI_SINE:
			n = 1 + brightness * 20;
			return -cos(2 * n * M_PI * phase);
			//return sin((n - 0.5) * M_PI + 2 * n * M_PI * phase);
		case TRIANGLE:
			return (phase >= 0.5) ? rescalef(phase, 0.5, 1.0, 1.0, -1.0) : rescalef(phase, 0.0, 0.5, -1.0, 1.0);
		case DOUBLE_TRIANGLE:
			phase = fmod(phase * 2, 1.0);
			return (phase >= 0.5) ? rescalef(phase, 0.5, 1.0, 1.0, -1.0) : rescalef(phase, 0.0, 0.5, -1.0, 1.0);
		case SAW:
			return rescalef(phase, 0.0, 1.0, -1.0, 1.0);
		case DOUBLE_SAW:
			phase = fmod(phase * 2, 1.0);
			return rescalef(phase, 0.0, 1.0, -1.0, 1.0);
		case TRAPEZOID:
			return clampf(((phase >= 0.5) ? rescalef(phase, 0.5, 1.0, 1.0, -1.0) : rescalef(phase, 0.0, 0.5, -1.0, 1.0)) * 2.0, -1.0, 1.0);
		case DOUBLE_TRAPEZOID:
			phase = fmod(phase * 2, 1.0);
			return clampf(((phase >= 0.5) ? rescalef(phase, 0.5, 1.0, 1.0, -1.0) : rescalef(phase, 0.0, 0.5, -1.0, 1.0)) * 2.0, -1.0, 1.0);
		case SQUARE:
			return (phase > 0.25 && phase <= 0.75) ? 1.0 : -1.0;
		case DOUBLE_SQUARE:
			phase = fmod(phase * 2, 1.0);
			return (phase > 0.25 && phase <= 0.75) ? 1.0 : -1.0;
		case PULSE:
			return (phase == 0.25) ? 1.0 : -1.0;
		case DOUBLE_PULSE:
			return (phase == 0.25 || phase == 0.75) ? 1.0 : -1.0;
		default:
			return sin(-0.5 * M_PI + 2 * M_PI * phase);
	}
	*/
};


void BaseWave::updatePhasor() {
	// Generate phasor function using shape, angle and level
	float points[8] = {};
	int num_points = 1;
	// Start point is implicitly created in 0, 0
	
	if (bottom_magnitude > 0.0) {
		float l;
		if (bottom_angle <= 0.5) {
			l = bottom_magnitude / cos(bottom_angle * M_PI / 2.0);
			bottom_x = l * sin(bottom_angle * M_PI / 2.0);
			bottom_y = bottom_magnitude;
		}
		else {
			l = bottom_magnitude / sin(bottom_angle * M_PI / 2.0);
			bottom_x = bottom_magnitude;
			bottom_y = l * cos(bottom_angle * M_PI / 2.0);
		}
		points[num_points * 2]     = bottom_x;
		points[num_points * 2 + 1] = bottom_y;
		num_points++;
	}
	else {
		points[num_points * 2]     = 0.0;
		points[num_points * 2 + 1] = 0.0;
		num_points++;

	}
	
	// Second point is calculated exactly the same as first, but it's orient relative to 1,1 point rather than 0,0
	if (top_magnitude > 0.0) {
		float l;
		if (top_angle <= 0.5) {
			l = top_magnitude / cos(top_angle * M_PI / 2.0);
			top_x = 1.0 - l * sin(top_angle * M_PI / 2.0);
			top_y = 1.0 - top_magnitude;
		}
		else {
			l = top_magnitude / sin(top_angle * M_PI / 2.0);
			top_x = 1.0 - top_magnitude;
			top_y = 1.0 - l * cos(top_angle * M_PI / 2.0);
		}
		points[num_points * 2]     = top_x;
		points[num_points * 2 + 1] = top_y;
		num_points++;
	}
	else {
		points[num_points * 2]     = 1.0;
		points[num_points * 2 + 1] = 1.0;
		num_points++;
	};
	
	// End point in top right corner
	points[num_points * 2]     = 1.0;
	points[num_points * 2 + 1] = 1.0;
	num_points++;
	
	for (int point = 1; point < num_points; point++) {
		float x1 = points[(point - 1) * 2];
		float y1 = points[(point - 1) * 2 + 1];
		float x2 = points[point * 2];
		float y2 = points[point * 2 + 1];
		float dx = x2 - x1;
		float dy = y2 - y1;
		for (float x = x1 * WAVE_LEN; x <= x2 * WAVE_LEN; x = x + 1.0) {
			phasor[(int) x] = dx ? (y1 + dy * (x / WAVE_LEN - x1) / dx): y2;
		};
	}
	
	if (bezier_ratio > 0.0) {
		// Generate Bezier curve
		float bezier[WAVE_LEN] = {};
		const int max_weight = 4;
		float weight = 1 + bezier_weight * max_weight;
		for(float u = 0 ; u < 1 ; u += 1.0 / WAVE_LEN / 16) {
			float bx = pow(1 - u, 3) * points[0] + weight * 3 * u * pow(1 - u, 2) * points[2] + weight * 3 * pow(u, 2) * (1 - u) * points[4] + pow(u, 3) * points[6]; 
			float by = pow(1 - u, 3) * points[1] + weight * 3 * u * pow(1 - u, 2) * points[3] + weight * 3 * pow(u, 2) * (1 - u) * points[5] + pow(u, 3) * points[7]; 
			float bw = pow(1 - u, 3) + weight * 3 * u * pow(1 - u, 2) + weight * 3 * pow(u, 2) * (1 - u) + pow(u, 3); 
			
			bx = clampf(bx / bw, 0.0, 1.0);
			by = clampf(by / bw, -1.0, 1.0);
			bezier[(int) (bx * WAVE_LEN)] = by;
			
			
		}
		
		
		// Crossfade with line-based phasor with Bezier curve
		for (int i = 0; i < WAVE_LEN; i++) 
			phasor[i] = crossf(phasor[i], bezier[i], bezier_ratio);
		
	}

};


// Apply phasor to base wave shape
void BaseWave::generateSamples() {
	const int MAX_RESONANCE = 4;
	float tmp[WAVE_LEN + 1];
	//memcpy(tmp, phasor, sizeof(float) * WAVE_LEN);
	//tmp[WAVE_LEN] = tmp[0];
	
	float final_phasor[WAVE_LEN];
	float tmp_samples[WAVE_LEN];
	
	if (resonance > 0.0) {
		float phase = 0.0;
		float sync_phase = 0.0;
		for (int i = 0; i < WAVE_LEN; i++) {
			final_phasor[i] = phasor[(int)(sync_phase * WAVE_LEN)];
			//final_phasor[i] = linterpf(phasor, sync_phase * WAVE_LEN);
//			samples[i] = linterpf(tmp, phasor[(int)(sync_phase * WAVE_LEN)] * WAVE_LEN);
			phase += 1.0 / WAVE_LEN;
			sync_phase += 1.0 / WAVE_LEN * (1 + MAX_RESONANCE * resonance);
			if (sync_phase >= 1.0)
				sync_phase -= 1.0;
				
		}
		/*
		for (int i = 0; i < WAVE_LEN + 1; i++) {
			tmp[i] = rescalef(tmp[i], -1.0, 1.0, 0.0, 1.0);
		};
		// Hello, 1985!
		float a = 0.0, b = 0.0, c = 0.0, d = 0.0, e = 0.0;
		// (a) The base frequency counter, wrapping around every period.
		// (b) The resonance frequency counter at a slightly higher frequency, being reset (or "synced") when the base counter wraps around.
		// (c) The resonance frequency counter used as a sine wave readout. Note the nasty sudden jump at the reset!
		// (d) The inverted base frequency counter.
		// (e) Multiplying c by d. The sudden jump in c is now leveled out.
		float step = 1.0 / WAVE_LEN;
		for (int i = 0; i < WAVE_LEN; i++) {
			a = phasor[i];
			//a += step;
			b += step * (1 + MAX_RESONANCE * resonance);
			if (b >= 1.0)
				b = 0.0;
			//fmod(a * (1 + MAX_RESONANCE * resonance), 1.0);
		
			c = linterpf(tmp, b * WAVE_LEN);
			//c = tmp[(int) (b * WAVE_LEN)];//linterpf(shape, b * WAVE_LEN);
			d = 1.0 - a;
			e = c * d;
			samples[i] = rescalef(e, 0.0, 1.0, -1.0, 1.0);
		};
		*/
	}
	else {
		memcpy(final_phasor, phasor, sizeof(float) * WAVE_LEN);
		//for (int i = 0; i < WAVE_LEN; i++)
		//	samples[i] = linterpf(tmp, phasor[i] * WAVE_LEN);
		
	};
	
	generateShape(final_phasor, tmp_samples);
	
	if (resonance > 0.0) {
		for (int i = 0; i < WAVE_LEN; i++) {
			tmp_samples[i] = rescalef(rescalef(tmp_samples[i], -1.0, 1.0, 0.0, 1.0) * (WAVE_LEN - i) / WAVE_LEN, 0.0, 1.0, -1.0, 1.0);
		};
	}
	
	
	normalize_array(tmp_samples, WAVE_LEN, -1.0, 1.0, 0.0);
	
	
	// Convert wave to spectrum
	RFFT(tmp_samples, tmp, WAVE_LEN);
	// Convert spectrum to harmonics
	for (int i = 0; i < WAVE_LEN / 2; i++) {
		harmonics[i] = hypotf(tmp[2 * i], tmp[2 * i + 1]) * 2.0;
	};
	memcpy(samples, tmp_samples, sizeof(float) * WAVE_LEN);
	updateSamples();
};


void BaseWave::updateSamples() {
	for (int j = 0; j < BANK_LEN; j++) {
		memcpy(currentBank.waves[j].samples, samples, sizeof(float) * WAVE_LEN);
		currentBank.waves[j].commitSamples();
	}
};

void BaseWave::commitSamples() {
	generateSamples();
};


void BaseWave::commitShape() {
	generateSamples();
};


void BaseWave::commitPhasor() {
	generateSamples();
};
