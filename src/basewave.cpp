#include "WaveEdit.hpp"
#include <string.h>


Oscillator osc;


void BaseWave::clear() {
	memset(this, 0, sizeof(BaseWave));
	lower_shape = SINE;
	upper_shape = SINE;
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
	resonance = 0.0;
	multi_algo = MUL_RESONANT;
	updateShape();
	updatePhasor();
	generateSamples(false);
}


void BaseWave::updateShape() {	
	float linear_phasor[WAVE_LEN];
	for (int i = 0; i < WAVE_LEN; i++)
		linear_phasor[i] = (float) i / WAVE_LEN;
	
	// Generate base wave shape using lower/upper wave shapes and brightness
	float flshape = lower_shape * (WAVE_SHAPES_LEN - 1);
	float fushape = upper_shape * (WAVE_SHAPES_LEN - 1); 
	float flshape1 = floor(flshape);
	float fushape1 = floor(fushape);
	float flshape2 = fmod(flshape1 + 1.0, WAVE_SHAPES_LEN);
	float fushape2 = fmod(fushape1 + 1.0, WAVE_SHAPES_LEN);
	flshape = fmod(flshape, 1.0);
	fushape = fmod(fushape, 1.0);

	osc.dt = powf(2.0, (1.0 - clampf(brightness, 0.0, 1.0)) * 4) / (float) WAVE_LEN;
	osc.pulse_width = clampf(pulse_width, 0.0, 1.0);	
	osc.render(
		(WaveShapeID) (int) flshape1, (WaveShapeID) (int) flshape2,  flshape,
		(WaveShapeID) (int) fushape1, (WaveShapeID) (int) fushape2,  fushape,
		shape);
}	


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
void BaseWave::generateSamples(bool update_waves) {
	const int MAX_RESONANCE = 4;
	float tmp[WAVE_LEN + 1];
	memcpy(tmp, phasor, sizeof(float) * WAVE_LEN);
	tmp[WAVE_LEN] = 1.0;

	float final_phasor[WAVE_LEN];
	float envelope[WAVE_LEN];
	float tmp_samples[WAVE_LEN];
	
	if (resonance > 0.0) {
		/*
		float total_levels = powf(2.0, resonance * 3);
		float int_levels = powf(2.0, floor(resonance * 3));
		float rem_levels = total_levels - int_levels;
		float int_width = (float) WAVE_LEN / total_levels;
		//float int_width = full_width * int_levels / total_levels;
		float full_width = (
		float rem_width = (float) WAVE_LEN / rem_levels;
		printf("%f %f %f %f\n", total_levels, int_levels, int_width, rem_width);
		for (int i = 0; i < WAVE_LEN; i++) {
			float phase = (float) i;
			
			
			int step = (int) (phase / full_width);
			float step_phase = fmod(phase, full_width);
			float full_pos = fmod(phase, full_width);
			float int_pos = fmod(full_pos, int_width);
			float rem_pos = fmod(full_pos, int_width);
			
			//printf("%i %f %f %f\n", i, full_pos, int_pos, rem_pos);
			
			if (step_phase < int_width) {
				final_phasor[i] = linterpf(tmp, rescalef(int_pos, 0, int_width, 0, WAVE_LEN));
			}
			else {
				//printf("%i RP %f RW %f FP %f FW %f IP %f IW %f\n", i, rem_pos, rem_width, full_pos, full_width, int_pos, int_width);
				//printf("%f\n", rescalef(rem_pos, 0, rem_width, 0, WAVE_LEN));
				final_phasor[i] = linterpf(tmp, rescalef(rem_pos, 0, rem_width, 0, WAVE_LEN));
			}
			
		}
		*/
		if (multi_algo == MUL_RESONANT) {
			float phase = 0.0;
			float sync_phase = 0.0;
			for (int i = 0; i < WAVE_LEN; i++) {
				final_phasor[i] = linterpf(tmp, sync_phase * WAVE_LEN);
				phase = (float)i / WAVE_LEN;
				sync_phase += 1.0 / WAVE_LEN * (1 + MAX_RESONANCE * resonance);
				if (sync_phase >= 1.0)
					sync_phase -= 1.0;
				envelope[i] = (float) (WAVE_LEN - i) / WAVE_LEN;
			}
		}
		else if (multi_algo == MUL_DIV_MOD) {
			float total_levels = rescalef(resonance, 0.0, 1.0, 1.0, 8.0);
			float int_levels = floor(total_levels);
			float rem_levels = total_levels - int_levels;
			for (int i = 0; i < WAVE_LEN; i++) {
				float pos = (total_levels) * i / WAVE_LEN;
				if (pos <= int_levels) {
					final_phasor[i] = fmod(pos, 1.0);
					envelope[i] = 1.0;
				}
				else {
					final_phasor[i] = fmod(pos, 1.0) / rem_levels;
					envelope[i] = rem_levels;
				}
			};
		}
		else if (multi_algo == MUL_HARMONIC) {
            float total_levels = round(1 + resonance * 15);
            for (int i = 0; i < WAVE_LEN; i++) {
                final_phasor[i] = wrap(total_levels * i / WAVE_LEN, 1.0);
                envelope[i] = 1.0;
            }
		}

	}
	else {
		memcpy(final_phasor, phasor, sizeof(float) * WAVE_LEN);
		
	};
	
	// TODO: maybe allow passing phasor to shape render func?
	memcpy(tmp, shape, sizeof(float) * WAVE_LEN);
	tmp[WAVE_LEN] = tmp[0];
	for (int i = 0; i < WAVE_LEN; i++)
		tmp_samples[i] = linterpf(tmp, WAVE_LEN * final_phasor[i]);
	
	if (resonance > 0.0) {
		for (int i = 0; i < WAVE_LEN; i++) {
			tmp_samples[i] = rescalef(rescalef(tmp_samples[i], -1.0, 1.0, 0.0, 1.0) * envelope[i], 0.0, 1.0, -1.0, 1.0);
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

	updateSamples(update_waves);
};


void BaseWave::updateSamples(bool copy_samples) {
	for (int j = 0; j < BANK_LEN; j++) {
		if (copy_samples)
			memcpy(currentBank.waves[j].samples, samples, sizeof(float) * WAVE_LEN);
		currentBank.waves[j].commitSamples();
	}
};

