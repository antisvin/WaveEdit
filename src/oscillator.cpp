#include "WaveEdit.hpp"
#include <string.h>


void Oscillator::render(WaveShapeID lower_a, WaveShapeID lower_b, float lower_ratio, WaveShapeID upper_a, WaveShapeID upper_b, float upper_ratio, float *samples) {
	float compensation_a[WAVE_LEN] = {};
	float compensation_b[WAVE_LEN] = {};
	float samples_a[WAVE_LEN] = {};
	float samples_b[WAVE_LEN] = {};
	
	if (lower_a == upper_a && lower_b == upper_b && lower_ratio == upper_ratio) {
		// Both shapes are the same, so we can run simplified code here
		getWave(lower_a, true, true, samples_a, compensation_a);
		getWave(lower_b, true, true, samples_b, compensation_b);
		for (int i = 0; i < WAVE_LEN; i++)
			samples[i] = crossf(samples_a[i] + compensation_a[i], samples_b[i] + compensation_b[i], lower_ratio);
	}
	else {
		// This case is more complicated. We have to calculate both waves, but wave generators would be
		// returning just one part of their wave. However, we will be getting blamp calculation results
		// past mid point for some wave i.e. triangle.
		
		getWave(lower_a, true, false, samples_a, compensation_a);
		getWave(lower_b, true, false, samples_b, compensation_b);
		for (int i = 0; i < WAVE_LEN; i++)
			samples[i] = crossf(samples_a[i] + compensation_a[i], samples_b[i] + compensation_b[i], lower_ratio);
		
		memset(samples_a, 0, sizeof(float) * WAVE_LEN);
		memset(samples_b, 0, sizeof(float) * WAVE_LEN);
		memset(compensation_a, 0, sizeof(float) * WAVE_LEN);
		memset(compensation_b, 0, sizeof(float) * WAVE_LEN);
		
		getWave(upper_a, false, true, samples_a, compensation_a);
		getWave(upper_b, false, true, samples_b, compensation_b);
		for (int i = 0; i < WAVE_LEN; i++)
			samples[i] += crossf(samples_a[i] + compensation_a[i], samples_b[i] + compensation_b[i], upper_ratio);
	};
	
};


void Oscillator::getWave(WaveShapeID shape, bool lower, bool upper, float *samples, float *compensation) {
	switch (shape) {
		case SINE:
			sineWave(lower, upper, samples, compensation);
			break;
		case HALF_SINE:
			halfSineWave(lower, upper, samples, compensation);
			break;
		case TRIANGLE:
			triangleWave(lower, upper, samples, compensation);
			break;
		case TRI_PULSE:
			triPulseWave(lower, upper, samples, compensation);
			break;
		case SQUARE:
			squareWave(lower, upper, samples, compensation);
			break;
		case RECTANGLE:
			rectangleWave(lower, upper, samples, compensation);
			break;
		case TRAPEZOID:
			trapezoidWave(lower, upper, samples, compensation);
			break;
		default:
			break;
	};
	normalize_array(samples, WAVE_LEN, -1.0, 1.0, 0.0);
};


void Oscillator::sineWave(bool lower, bool upper, float *samples, float *compensation) {
	float phase;
	for (int i = 0; i < WAVE_LEN; i++) {
		phase = (float)i / WAVE_LEN;
		
		if (phase < pulse_width && lower) {
			samples[i] = sin(M_PI * (phase / pulse_width - 0.5));
		}
		else if (phase >= pulse_width && upper) {
			samples[i] = sin(M_PI * ((phase - pulse_width) / (1 - pulse_width) + 0.5));
		};
	}
};



void Oscillator::halfSineWave(bool lower, bool upper, float *samples, float *compensation) {
	float phase;
	float t1 = 0.5 * pulse_width;
	float t2 = pulse_width + (1 - pulse_width) * 0.5 ; //0.5 * pulse_width + 0.5;
	for (int i = 0; i < WAVE_LEN; i++) {
		phase = (float) i / WAVE_LEN;
		
		if (lower) {
			compensation[i] += 2 * M_PI * dt * blamp(fmod(t1 + 1 - phase, 1.0), dt);
			if (phase < pulse_width) {
				if (phase > t1) {
					samples[i] = 2 * sin(M_PI * (phase / pulse_width - 0.5)) - 1;
				}
				else {
					samples[i] = -1.0;
				};
			};

		};
		if (upper) {
			compensation[i] += 2 * M_PI * dt * blamp(fmod(t2 + 1 - phase, 1.0), dt);
			if (phase >= pulse_width) {
				if (phase < t2) {
					samples[i] = 2 * sin(M_PI * ((phase - pulse_width) / (1 - pulse_width) + 0.5)) - 1;
				}
				else {
					samples[i] = -1.0;
				};
			};
		}
	}
	
};

void Oscillator::triangleWave(bool lower, bool upper, float *samples, float *compensation) {
	float phase;
	float t1 = 0.0;
	float t2 = pulse_width;

	for (int i = 0; i < WAVE_LEN; i++) {
		phase = (float) i / WAVE_LEN;
		
		compensation[i] = dt / (pulse_width - pulse_width * pulse_width) * (blamp(fmod(t1 + phase, 1.0), dt) - blamp(fmod(t2 + 1 - phase, 1.0), dt));
		
		if (lower && phase < pulse_width)
			samples[i] = 2.0 * phase / pulse_width - 1;		
		else if (upper && phase >= pulse_width)
			samples[i] =  1.0 - 2.0 * (phase - t2) / (1 - pulse_width);
	}
};

void Oscillator::triPulseWave(bool lower, bool upper, float *samples, float *compensation) {
	float phase;
	float t1 = 0.5 * pulse_width;
	float t2 = pulse_width;
	float t3 = pulse_width + 0.5 * (1 - pulse_width);

	for (int i = 0; i < WAVE_LEN; i++) {
		phase = (float) i / WAVE_LEN;
		
		compensation[i] = -dt / (pulse_width - pulse_width * pulse_width) * blamp(fmod(t2 + 1 - phase, 1.0), dt);

		if (lower) {
			compensation[i] += dt / (pulse_width - pulse_width * pulse_width) * blamp(fmod(1 + t1 - phase, 1.0), dt);
			if (phase < t1)
				samples[i] = -1;
			else if (phase >= t1 && phase < t2)
				samples[i] = rescalef(phase, t1, t2, -1, 1);
		};
		if (upper) {
			compensation[i] += dt / (pulse_width - pulse_width * pulse_width) * blamp(fmod(1 + t3 - phase, 1.0), dt);
			if (t2 <= phase && phase < t3)
				samples[i] = rescalef(phase, t2, t3, 1, -1);
			else if (phase >= t3)
				samples[i] = -1;
		}
	}
};


void Oscillator::squareWave(bool lower, bool upper, float *samples, float *compensation) {
	float phase;
	float t1 = 0.5 * pulse_width;
	float t2 = pulse_width + (1 - pulse_width) * 0.5;
	
	for (int i = 0; i < WAVE_LEN; i++) {
		phase = (float) i / WAVE_LEN;
		
		if (lower) {
			compensation[i] += blep(fmod(1 - t1 + phase, 1.0), dt);
			 if (phase < pulse_width)
				samples[i] = (phase < t1) ? -1 : 1;
		};
		if (upper) {
			compensation[i] -= blep(fmod(1 - t2 + phase, 1.0), dt);
			if (phase >= pulse_width)
				samples[i] = (phase < t2) ? 1 : -1;
		};
	}	
};

void Oscillator::rectangleWave(bool lower, bool upper, float *samples, float *compensation) {
	float phase;
	float t1 = 0.25 * pulse_width;
	float t2 = pulse_width - 0.25 * pulse_width;
	float t3 = pulse_width + (1 - pulse_width) * 0.25;
	float t4 = 1 - (1 - pulse_width) * 0.25;
	
	for (int i = 0; i < WAVE_LEN; i++) {
		phase = (float) i / WAVE_LEN;
		if (lower) {
			compensation[i] += blep(fmod(1 - t1 + phase, 1.0), dt) * 0.5;
			compensation[i] += blep(fmod(1 - t2 + phase, 1.0), dt) * 0.5;
			if (phase < pulse_width) {
				if (phase < t1) {
					samples[i] = -1;
				}
				else if (phase >= t2) {
					samples[i] = 1;
				}
				else {
					samples[i] = 0;
				}
			}
		};
		if (upper) {
			compensation[i] -= blep(fmod(1 - t3 + phase, 1.0), dt) * 0.5;
			compensation[i] -= blep(fmod(1 - t4 + phase, 1.0), dt) * 0.5;
			if (phase >= pulse_width) {
				if (phase < t3) {
					samples[i] = 1;
				}
				else if (phase >= t4) {
					samples[i] = -1;
				}
				else {
					samples[i] = 0;
				}
			}
		};
		
	}
};

void Oscillator::trapezoidWave(bool lower, bool upper, float *samples, float *compensation) {
	float phase;
	float t1 = 0.25 * pulse_width;
	float t2 = 0.25 + 0.75 * pulse_width;
	float scale =  1 / (1 - pulse_width);
		
	for (int i = 0; i < WAVE_LEN; i++) {
		phase = (float) i / WAVE_LEN;
		if (lower) {
			compensation[i] += 2 / pulse_width * dt * (
				blamp(fmod(1.0 - t1 + phase, 1.0), dt) -
				blamp(fmod(1.0 + pulse_width - t1 - phase, 1.0), dt));
			if (phase < pulse_width)
				samples[i] = clampf(rescalef(phase, t1, pulse_width - t1, -1.0, 1.0), -1.0, 1.0);
		};
		if (upper) {
			compensation[i] += 2  / (1 - pulse_width) *  dt * (
				blamp(fmod(1 - pulse_width + t2 + phase, 1.0), dt) -
				blamp(fmod(1.0 - t2 + phase, 1.0), dt));
			if (upper && phase >= pulse_width)
				samples[i] = clampf(rescalef(phase, t2, 1 + pulse_width - t2 , 1.0, -1.0), -1.0, 1.0);
		};
	}
};
