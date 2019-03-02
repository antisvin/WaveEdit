#include "WaveEdit.hpp"
#include <string.h>
#include <sndfile.h>


static Wave clipboardWave = {};
bool clipboardActive = false;


const char *effectNames[EFFECTS_LEN] {
	"Pre-Gain",
	"Phase Shift",
	"Harmonic Shift",
	"Harmonic Asymetry",
	"Harmonic Balance",
	"Harmonic Stretch",
	"Harmonic Fold",
	"Phase Distortion",
	"Cubic Distortion",
	"Comb Filter",
	"Chebyshev Wavefolding",
	"Sample & Hold",
	"Track & Hold",
	"Quantization",
	"Slew Limiter",
	"Lowpass Filter",
	"Highpass Filter",
	"Phase Modulation Feedback",
	"Frequency Modulation Feedback",
	"Ring Modulation Feedback",
	"Amplitude Modulation Feedback",
	"Modulation Index",
	"Low Boost",
	"Mid Boost",
	"High Boost",
	"Post-Gain",
};


void Wave::clear() {
	memset(this, 0, sizeof(Wave));
}

void Wave::updatePost() {
	float out[WAVE_LEN];
	memcpy(out, samples, sizeof(float) * WAVE_LEN);

	// Pre-gain with saturation / soft clipping
	if (effects[PRE_GAIN]) {
		float gain = powf(20.0, effects[PRE_GAIN]);
		float tmp[WAVE_LEN];
		memcpy(tmp, out, sizeof(float) * WAVE_LEN);
		for (int i = 0; i < WAVE_LEN; i++) {
			out[i] *= gain;
			if (fabs(out[i]) >= 1.0)
				out[i] = clampf(out[i], -2.0 / 3.0, 2.0 / 3.0);
			else
				out[i] *= (1 - out[i] * out[i] / 3.0);
			out[i] = crossf(tmp[i], out[i] * 1.5, effects[PRE_GAIN]);
		}
	}

	// Temporal and Harmonic Shift, Harmonic Asymetry, Harmonic Balance, Harmonic Stretch
	if (effects[HARMONIC_STRETCH] > 0.0 || effects[PHASE_SHIFT] > 0.0 || effects[HARMONIC_ASYMETRY] > 0.0 || effects[HARMONIC_BALANCE] > 0.0 || effects[HARMONIC_SHIFT] > 0.0 || effects[HARMONIC_FOLD] > 0.0) {
		// Shift Fourier phase proportionally
		float tmp[WAVE_LEN];
		float tmp1[WAVE_LEN] = {};
		float *tmp2;
		float tmp3[WAVE_LEN] = {};
		RFFT(out, tmp, WAVE_LEN);
		for (int k = 0; k < WAVE_LEN / 2; k++) {
			float phase = clampf(effects[HARMONIC_SHIFT], 0.0, 1.0) + clampf(effects[PHASE_SHIFT], 0.0, 1.0) * k;
			float br = cosf(2 * M_PI * phase);
			float bi = -sinf(2 * M_PI * phase);
			cmultf(&tmp[2 * k], &tmp[2 * k + 1], tmp[2 * k], tmp[2 * k + 1], br, bi);
			if ((effects[HARMONIC_ASYMETRY] > 0.0 || effects[HARMONIC_BALANCE] > 0.0) && k > 1) {
				float phase = clampf(effects[HARMONIC_SHIFT], 0.0, 1.0) + clampf(effects[PHASE_SHIFT], 0.0, 1.0) * k;
				float br = cosf(2 * M_PI * phase);
				float bi = -sinf(2 * M_PI * phase);
				cmultf(&tmp[2 * k], &tmp[2 * k + 1], tmp[2 * k], tmp[2 * k + 1], br, bi);
				if (k % 2 == 0) {
					float mag1 = hypotf(tmp[2 * k], tmp[2 * k + 1]);
					float mag2 = hypotf(tmp[2 * k + 2], tmp[2 * k + 3]);
					if (effects[HARMONIC_BALANCE] > 0.0) {
						float mag1_ratio = mag1 ? (1 - (mag1 - mag2) * effects[HARMONIC_BALANCE] / mag1) : 0.0; 
						float mag2_ratio = mag2 ? (1 - (mag2 - mag1) * effects[HARMONIC_BALANCE] / mag2) : 0.0; 
						tmp[2 * k] *= mag1_ratio;
						tmp[2 * k + 1] *= mag1_ratio;
						tmp[2 * k + 2] *= mag2_ratio;
						tmp[2 * k + 3] *= mag2_ratio;
					}
					if (effects[HARMONIC_ASYMETRY] > 0.0) {
						float mag_min = fminf(mag1, mag2) * effects[HARMONIC_ASYMETRY];
						float mag1_ratio = mag1 ? ((mag1 - mag_min) / mag1) : 0.0;
						float mag2_ratio = mag2 ? ((mag2 - mag_min) / mag2) : 0.0;
						tmp[2 * k] *= mag1_ratio;
						tmp[2 * k + 1] *= mag1_ratio;
						tmp[2 * k + 2] *= mag2_ratio;
						tmp[2 * k + 3] *= mag2_ratio;
					}
				}
			};
			if (effects[HARMONIC_STRETCH] > 0.0 && k < WAVE_LEN) {
				const int steps = 8;
				float scale = effects[HARMONIC_STRETCH] * steps;
				float dstf = fmod(k + k * scale, WAVE_LEN / 2);
				int dst = (int) dstf * 2;
				float ratio = fmod(dstf, 1.0);
				tmp1[dst % WAVE_LEN] += crossf(tmp[2 * k], 0.0, ratio);
				tmp1[(dst + 1) % WAVE_LEN] += crossf(tmp[(2 * k + 1) % WAVE_LEN], 0.0, ratio);
				tmp1[(dst + 2) % WAVE_LEN] += crossf(0.0, tmp[2 * k], ratio);
				tmp1[(dst + 3) % WAVE_LEN] += crossf(0.0, tmp[(2 * k + 1) % WAVE_LEN], ratio);
			}
		};
		if (effects[HARMONIC_STRETCH] > 0.0) {
			tmp2 = tmp1;
		}
		else {
			tmp2 = tmp;
		}
		if (effects[HARMONIC_FOLD] > 0.0){
			float limit = rescalef(clampf(effects[HARMONIC_FOLD], 0.0, 1.0), 0.0, 1.0, (float) WAVE_LEN / 2, 1.0);
			int ilimit = (int) limit;
			float ratio = 1.0 - fmod(limit, 1.0);
			for (int i = 0; i < ilimit * 2; i++) {
				tmp3[i] = tmp2[i];
			};
			for (int i = ilimit; i < WAVE_LEN / 2; i++){
				int dst = i;
				if (dst >= ilimit * 2)
					dst = fmod(dst, ilimit * 2.0);
				if (dst > ilimit)
					dst = ilimit * 2 - dst;
				tmp3[dst * 2] += tmp2[i * 2] * ratio;
				tmp3[dst * 2 + 1] += tmp2[i * 2 + 1] * ratio;
				tmp3[dst * 2 + 2] += tmp2[i * 2] * (1.0 - ratio);
				tmp3[dst * 2 + 3] += tmp2[i * 2 + 1] * (1.0 - ratio);
			};
			tmp2 = tmp3;
		}
		IRFFT(tmp2, out, WAVE_LEN);
	}
	
	if (effects[PHASE_DISTORTION] > 0.0 || effects[CUBIC_DISTORTION] > 0.0) {
		float phase, dst_phase;
		float tmp[WAVE_LEN + 1];
		memcpy(tmp, out, sizeof(float) * WAVE_LEN);
		tmp[WAVE_LEN] = tmp[0];
		
		float phase_midpoint = 0.5 + clampf(effects[PHASE_DISTORTION], 0.0, 1.0) / 2;
		for (int i = 0; i < WAVE_LEN; i++) {
			phase = ((float) i ) / WAVE_LEN;
			if (phase < phase_midpoint) {
				dst_phase = rescalef(phase, 0.0, phase_midpoint, 0.0, 0.5);
			}
			else {
				dst_phase = rescalef(phase, phase_midpoint, 1.0, 0.5, 1.0);
			};
			
			float cubic_phase = rescalef(dst_phase, 0.0, 1.0, -1.0, 1.0);
			cubic_phase = cubic_phase * cubic_phase * cubic_phase;
			
			float final_phase = crossf(dst_phase, rescalef(cubic_phase, -1.0, 1.0, 0.0, 1.0), effects[CUBIC_DISTORTION]);
			
			int dst_idx = (int) (final_phase * WAVE_LEN);
			float delta = (final_phase - ((float) dst_idx) / WAVE_LEN) * WAVE_LEN;
			out[i] = crossf(tmp[dst_idx], tmp[dst_idx + 1], delta);
		}
	}

	// Comb filter
	if (effects[COMB] > 0.0) {
		const float base = 0.75;
		const int taps = 40;

		// Build the kernel in Fourier space
		// Place taps at positions `comb * j`, with exponentially decreasing amplitude
		float kernel[WAVE_LEN] = {};
		for (int k = 0; k < WAVE_LEN / 2; k++) {
			for (int j = 0; j < taps; j++) {
				float amplitude = powf(base, j);
				// Normalize by sum of geometric series
				amplitude *= (1.0 - base);
				float phase = -2.0 * M_PI * k * effects[COMB] * j;
				kernel[2 * k] += amplitude * cosf(phase);
				kernel[2 * k + 1] += amplitude * sinf(phase);
			}
		}

		// Convolve FFT of input with kernel
		float fft[WAVE_LEN];
		RFFT(out, fft, WAVE_LEN);
		for (int k = 0; k < WAVE_LEN / 2; k++) {
			cmultf(&fft[2 * k], &fft[2 * k + 1], fft[2 * k], fft[2 * k + 1], kernel[2 * k], kernel[2 * k + 1]);
		}
		IRFFT(fft, out, WAVE_LEN);
	}

	// Chebyshev waveshaping
	if (effects[CHEBYSHEV] > 0.0) {
		float n = powf(50.0, effects[CHEBYSHEV]);
		for (int i = 0; i < WAVE_LEN; i++) {
			// Apply a distant variant of the Chebyshev polynomial of the first kind
			if (-1.0 <= out[i] && out[i] <= 1.0)
				out[i] = sinf(n * asinf(out[i]));
			else
				out[i] = sinf(n * asinf(1.0 / out[i]));
		}
	}

	// Sample & Hold
	if (effects[SAMPLE_AND_HOLD] > 0.0) {
		float frameskip = powf(WAVE_LEN / 2.0, clampf(effects[SAMPLE_AND_HOLD], 0.0, 1.0));
		float tmp[WAVE_LEN + 1];
		memcpy(tmp, out, sizeof(float) * WAVE_LEN);
		tmp[WAVE_LEN] = tmp[0];

		// Dumb linear interpolation S&H
		for (int i = 0; i < WAVE_LEN; i++) {
			float index = roundf(i / frameskip) * frameskip;
			out[i] = linterpf(tmp, clampf(index, 0.0, WAVE_LEN - 1));
		}
	}

	// Track & Hold
	if (effects[TRACK_AND_HOLD] > 0.0) {
		float frameskip = powf(WAVE_LEN / 2.0, clampf(effects[TRACK_AND_HOLD], 0.0, 1.0));
		float tmp[WAVE_LEN + 1];
		memcpy(tmp, out, sizeof(float) * WAVE_LEN);
		tmp[WAVE_LEN] = tmp[0];

		// Dumb linear interpolation S&H
		for (int i = 0; i < WAVE_LEN; i++) {
			float index = roundf(i / frameskip) * frameskip;
			if (i >= index) {
			    out[i] = linterpf(tmp, clampf(index, 0.0, WAVE_LEN - 1));
			}
		}
	}

	// Quantization
	if (effects[QUANTIZATION] > 1e-3) {
		float levels = powf(clampf(effects[QUANTIZATION], 0.0, 1.0), -1.5);
		for (int i = 0; i < WAVE_LEN; i++) {
			out[i] = roundf(out[i] * levels) / levels;
		}
	}

	// Slew Limiter
	if (effects[SLEW] > 0.0) {
		float slew = powf(0.001, effects[SLEW]);

		float y = out[0];
		for (int i = 1; i < WAVE_LEN; i++) {
			float dxdt = out[i] - y;
			float dydt = clampf(dxdt, -slew, slew);
			y += dydt;
			out[i] = y;
		}
	}

	// Brick-wall lowpass / highpass filter
	// TODO Maybe change this into a more musical filter
	if (effects[LOWPASS] > 0.0 || effects[HIGHPASS]) {
		float fft[WAVE_LEN];
		RFFT(out, fft, WAVE_LEN);
		float lowpass = 1.0 - effects[LOWPASS];
		float highpass = effects[HIGHPASS];
		for (int i = 1; i < WAVE_LEN / 2; i++) {
			float v = clampf(WAVE_LEN / 2 * lowpass - i, 0.0, 1.0) * clampf(-WAVE_LEN / 2 * highpass + i, 0.0, 1.0);
			fft[2 * i] *= v;
			fft[2 * i + 1] *= v;
		}
		IRFFT(fft, out, WAVE_LEN);
	}
	
	// Modulation Index doesn't have it's own effect, but is used by 4 modulation effects below.
	
	// Phase Feedback
	if (effects[PHASE_FEEDBACK] > 0.0) {
		phaseModulation(out, out, rescalef(clampf(effects[MODULATION_INDEX], 0.0, 1.0), 0.0, 1.0, 0.0, 4.0), clampf(effects[PHASE_FEEDBACK], 0.0, 1.0));
	}
	
	// Frequency Feedback
	if (effects[FREQUENCY_FEEDBACK] > 0.0) {
		frequencyModulation(out, out, rescalef(clampf(effects[MODULATION_INDEX], 0.0, 1.0), 0.0, 1.0, 0.0, 4.0), clampf(effects[FREQUENCY_FEEDBACK], 0.0, 1.0));
	}
	
	// Ring Feedback
	if (effects[RING_FEEDBACK] > 0.0) {
		ringModulation(out, out, rescalef(clampf(effects[MODULATION_INDEX], 0.0, 1.0), 0.0, 1.0, 1.0, 9.0), clampf(effects[RING_FEEDBACK], 0.0, 1.0));
	}
	
	// Amplitude Feedback
	if (effects[AMPLITUDE_FEEDBACK] > 0.0) {
		amplitudeModulation(out, out, rescalef(clampf(effects[MODULATION_INDEX], 0.0, 1.0), 0.0, 1.0, 1.0, 9.0), clampf(effects[AMPLITUDE_FEEDBACK], 0.0, 1.0));
	}
	
	if (effects[LOW_BOOST] > 0.0 || effects[MID_BOOST] > 0.0 || effects[HIGH_BOOST] > 0.0) {
		// Generate boost factors for every harmonic
		float boost[WAVE_LEN / 2];
		const float boost_level = 4.0;
		for (int i = 0; i < WAVE_LEN / 2; i++)
			boost[i] = 1.0;
		// The lower harmonic, the higher is its boost factor and only lowest half of spectrum is boosted.
		if (effects[LOW_BOOST] > 0.0)
			for (int i = 0; i < WAVE_LEN / 4; i++)
				boost[i] += boost_level * effects[LOW_BOOST] * (WAVE_LEN / 4 - i) / WAVE_LEN * 4.0;
		// The higher harmonic, the higher is its boost factor and only highest half of spectrum is boosted
		if (effects[HIGH_BOOST] > 0.0)
			for (int i = WAVE_LEN / 4; i < WAVE_LEN / 2; i++)
				boost[i] += boost_level * effects[HIGH_BOOST] * (1 + i - WAVE_LEN / 4) / WAVE_LEN * 4.0;
		// The closer harmonic is to the center, the higher is its boost factor. All spectrum is boosted.
		// This effect is applied after the previous too and takes their boost into consideration.
		if (effects[MID_BOOST] > 0.0) {
			for (int i = 0; i < WAVE_LEN / 4; i++){
				boost[i] *= (1 + boost_level * effects[MID_BOOST] * (i + 1) / WAVE_LEN * 4.0);
			};
			for (int i = WAVE_LEN / 4; i < WAVE_LEN / 2; i++){
				boost[i] *= (1 + boost_level * effects[MID_BOOST] * (WAVE_LEN / 2 - i) / WAVE_LEN * 4.0);
			}
		}
		// FFT transform
		float fft[WAVE_LEN];
		RFFT(out, fft, WAVE_LEN);
		
		// Multiply harmonics by boost factors
		for (int i = 0; i < WAVE_LEN / 2; i++) {
			fft[i * 2] *= boost[i];
			fft[i * 2 + 1] *= boost[i];
		}

		// Reverse FFT transform
		IRFFT(fft, out, WAVE_LEN);
	}
	
	// Post gain with saturation / soft clipping
	if (effects[POST_GAIN]) {
		float gain = powf(20.0, effects[POST_GAIN]);
		float tmp[WAVE_LEN];
		memcpy(tmp, out, sizeof(float) * WAVE_LEN);
		for (int i = 0; i < WAVE_LEN; i++) {
			out[i] *= gain;
			if (fabs(out[i]) >= 1.0)
				out[i] = clampf(out[i], -2.0 / 3.0, 2.0 / 3.0);
			else
				out[i] *= (1 - out[i] * out[i] / 3.0);
			out[i] = crossf(tmp[i], out[i] * 1.5, effects[POST_GAIN]);
		}
	}

	// Cycle
	if (cycle) {
		float start = out[0];
		float end = out[WAVE_LEN - 1] / (WAVE_LEN - 1) * WAVE_LEN;

		for (int i = 0; i < WAVE_LEN; i++) {
			out[i] -= (end - start) * (i - WAVE_LEN / 2) / WAVE_LEN;
		}
	}

	// Normalize
	if (normalize)
		normalize_array(out, WAVE_LEN, -1.0, 1.0, 0.0);

	// Hard clip :(
	for (int i = 0; i < WAVE_LEN; i++) {
		out[i] = clampf(out[i], -1.0, 1.0);
	}

	// TODO Fix possible race condition with audio thread here
	// Or not, because the race condition would only just replace samples as they are being read, which just gives a click sound.
	memcpy(postSamples, out, sizeof(float)*WAVE_LEN);

	// Convert wave to spectrum
	RFFT(postSamples, postSpectrum, WAVE_LEN);
	// Convert spectrum to harmonics
	for (int i = 0; i < WAVE_LEN / 2; i++) {
		postHarmonics[i] = hypotf(postSpectrum[2 * i], postSpectrum[2 * i + 1]) * 2.0;
	}
}

void Wave::commitSamples() {
	// Convert wave to spectrum
	RFFT(samples, spectrum, WAVE_LEN);
	// Convert spectrum to harmonics
	for (int i = 0; i < WAVE_LEN / 2; i++) {
		harmonics[i] = hypotf(spectrum[2 * i], spectrum[2 * i + 1]) * 2.0;
	}
	updatePost();
}

void Wave::commitHarmonics() {
	// Rescale spectrum by the new norm
	for (int i = 0; i < WAVE_LEN / 2; i++) {
		float oldHarmonic = hypotf(spectrum[2 * i], spectrum[2 * i + 1]);
		float newHarmonic = harmonics[i] / 2.0;
		if (oldHarmonic > 1.0e-6) {
			// Preserve old phase but apply new magnitude
			float ratio = newHarmonic / oldHarmonic;
			if (i == 0) {
				spectrum[2 * i] *= ratio;
				spectrum[2 * i + 1] = 0.0;
			}
			else {
				spectrum[2 * i] *= ratio;
				spectrum[2 * i + 1] *= ratio;
			}
		}
		else {
			// If there is no old phase (magnitude is 0), set to 90 degrees
			if (i == 0) {
				spectrum[2 * i] = newHarmonic;
				spectrum[2 * i + 1] = 0.0;
			}
			else {
				spectrum[2 * i] = 0.0;
				spectrum[2 * i + 1] = -newHarmonic;
			}
		}
	}
	// Convert spectrum to wave
	IRFFT(spectrum, samples, WAVE_LEN);
	updatePost();
}

void Wave::clearEffects() {
	memset(effects, 0, sizeof(float) * EFFECTS_LEN);
	cycle = false;
	normalize = true;
	updatePost();
}

void Wave::bakeEffects() {
	memcpy(samples, postSamples, sizeof(float)*WAVE_LEN);
	clearEffects();
	commitSamples();
}

void Wave::randomizeEffects() {
	for (int i = 0; i < EFFECTS_LEN; i++) {
		effects[i] = randf() > 0.75 ? powf(randf(), 2) : 0.0;
	}
	updatePost();
}

void Wave::morphEffect(Wave *from_wave, Wave *to_wave, EffectID effect, float fade) {
	effects[effect] = crossf(from_wave->effects[effect], to_wave->effects[effect], fade);
	updatePost();
}

void Wave::morphAllEffects(Wave *from_wave, Wave *to_wave, float fade) {
	for (int i = 0; i < EFFECTS_LEN; i++){
		effects[i] = crossf(from_wave->effects[i], to_wave->effects[i], fade);
	};
	updatePost();
}

void Wave::applyAmplitudeModulation(){
	amplitudeModulation(samples, clipboardWave.samples, 1.0, 1.0);
	updatePost();
}


void Wave::applyRingModulation(){
	ringModulation(samples, clipboardWave.samples, 1.0, 1.0);
	updatePost();
}

void Wave::applyPhaseModulation() {
	phaseModulation(samples, clipboardWave.samples, 0.0, 1.0);
        updatePost();
}

void Wave::applyFrequencyModulation() {
	frequencyModulation(samples, clipboardWave.samples, 1.0, 1.0);
        updatePost();
}

void Wave::applySpectralTransfer() {
	// Build the kernel in Fourier space
	float fft[WAVE_LEN];
	float kernel[WAVE_LEN];
	RFFT(clipboardWave.samples, kernel, WAVE_LEN);
	RFFT(samples, fft, WAVE_LEN);
	for (int k = 0; k < WAVE_LEN / 2; k++) {
		cmultf(&fft[2 * k], &fft[2 * k + 1], fft[2 * k], fft[2 * k + 1], kernel[2 * k], kernel[2 * k + 1]);
	}
	IRFFT(fft, samples, WAVE_LEN);
	updatePost();
}

void ringModulation(float *carrier, const float *modulator, float index, float depth) {
	const int oversample = 4;
	float tmp[WAVE_LEN * oversample + 1];
	float carrier_tmp[WAVE_LEN * oversample];
	float modulator_tmp[WAVE_LEN * oversample + 1];
	cyclicOversample(carrier, carrier_tmp, WAVE_LEN, oversample);
	cyclicOversample(modulator, modulator_tmp, WAVE_LEN, oversample);
	modulator_tmp[WAVE_LEN * oversample] = modulator_tmp[0];
	memcpy(tmp, carrier_tmp, sizeof(float) * WAVE_LEN * oversample);
	tmp[WAVE_LEN * oversample] = tmp[0];
	
	float index_mod = fmod(index, 1.0);
	if (index_mod == 0.0) index_mod = 1.0;
	
	for (int i = 0; i < WAVE_LEN * oversample; i++) {
		float modulation1 = linterpf(modulator_tmp, fmod(i * ceilf(index), WAVE_LEN * oversample + 1));
		float modulation2 = linterpf(modulator_tmp, fmod(i * ceilf(index + 1.0), WAVE_LEN * oversample + 1));
		//float mod_sample = linterpf(modulator, fmod((float)i * depth, WAVE_LEN));
		carrier_tmp[i] *= crossf(
			1.0,
			crossf(modulation1, modulation2, index_mod),
			depth);
		//(1 + rescalef(modulation1, -1.0, 1.0, 0.0, 1.0) * depth);
	};
	cyclicUndersample(carrier_tmp, carrier, WAVE_LEN * oversample, oversample);
	
//		float mod_sample = linterpf(modulator, fmod((float)i * index, WAVE_LEN));
//		carrier[i] *= mod_sample;
};

void amplitudeModulation(float *carrier, const float *modulator, float index, float depth) {
	const int oversample = 4;
	float tmp[WAVE_LEN * oversample + 1];
	float carrier_tmp[WAVE_LEN * oversample];
	float modulator_tmp[WAVE_LEN * oversample + 1];
	cyclicOversample(carrier, carrier_tmp, WAVE_LEN, oversample);
	cyclicOversample(modulator, modulator_tmp, WAVE_LEN, oversample);
	modulator_tmp[WAVE_LEN * oversample] = modulator_tmp[0];
	memcpy(tmp, carrier_tmp, sizeof(float) * WAVE_LEN * oversample);
	tmp[WAVE_LEN * oversample] = tmp[0];
	
	float index_mod = fmod(index, 1.0);
	if (index_mod == 0.0) index_mod = 1.0;
	
	for (int i = 0; i < WAVE_LEN * oversample; i++) {
		float modulation1 = linterpf(modulator_tmp, fmod(i * ceilf(index), WAVE_LEN * oversample + 1));
		float modulation2 = linterpf(modulator_tmp, fmod(i * ceilf(index + 1.0), WAVE_LEN * oversample + 1));
		//float mod_sample = linterpf(modulator, fmod((float)i * depth, WAVE_LEN));
		carrier_tmp[i] *= (1 + crossf(
			rescalef(modulation1, -1.0, 1.0, 0.0, depth),
			rescalef(modulation2, -1.0, 1.0, 0.0, depth),
			index_mod));
		//(1 + rescalef(modulation1, -1.0, 1.0, 0.0, 1.0) * depth);
	};
	cyclicUndersample(carrier_tmp, carrier, WAVE_LEN * oversample, oversample);
}

void phaseModulation(float *carrier, const float *modulator, float index, float depth) {
	const int oversample = 4;
	float tmp[WAVE_LEN * oversample + 1];
	float carrier_tmp[WAVE_LEN * oversample];
	float modulator_tmp[WAVE_LEN * oversample + 1];
	cyclicOversample(carrier, carrier_tmp, WAVE_LEN, oversample);
	cyclicOversample(modulator, modulator_tmp, WAVE_LEN, oversample);
	modulator_tmp[WAVE_LEN * oversample] = modulator_tmp[0];
	memcpy(tmp, carrier_tmp, sizeof(float) * WAVE_LEN * oversample);
	tmp[WAVE_LEN * oversample] = tmp[0];
	
	float index_mod = fmod(index, 1.0);
	if (index_mod == 0.0) index_mod = 1.0;
	
	for (int i = 0; i < WAVE_LEN * oversample; i++) {
		float modulation1 = linterpf(modulator_tmp, fmod(i * ceilf(index), WAVE_LEN * oversample + 1));
		float modulation2 = linterpf(modulator_tmp, fmod(i * ceilf(index + 1.0), WAVE_LEN * oversample + 1));
		float phase1 = ((float) i) / WAVE_LEN / oversample + modulation1  * depth;
		float phase2 = ((float) i) / WAVE_LEN / oversample + modulation2  * depth;
		if (phase1 <= 0)
			phase1 = 1 - phase1;
		if (phase2 <= 0)
			phase2 = 1 - phase2;
		carrier_tmp[i] = crossf(
			linterpf(tmp, fmod(phase1 * WAVE_LEN * oversample, WAVE_LEN * oversample + 1)),
			linterpf(tmp, fmod(phase2 * WAVE_LEN * oversample, WAVE_LEN * oversample + 1)),
			index_mod);
	};
	cyclicUndersample(carrier_tmp, carrier, WAVE_LEN * oversample, oversample);
}

void frequencyModulation(float *carrier, const float *modulator, float index, float depth) {
	const int oversample = 4;
	float tmp[WAVE_LEN * oversample + 1];
	float carrier_tmp[WAVE_LEN * oversample];
	float modulator_tmp[WAVE_LEN * oversample + 1];
	cyclicOversample(carrier, carrier_tmp, WAVE_LEN, oversample);
	cyclicOversample(modulator, modulator_tmp, WAVE_LEN, oversample);
	modulator_tmp[WAVE_LEN * oversample] = modulator_tmp[0];
	memcpy(tmp, carrier_tmp, sizeof(float) * WAVE_LEN * oversample);
	tmp[WAVE_LEN * oversample] = tmp[0];
	
	float index_mod = fmod(index, 1.0);
	if (index_mod == 0.0) index_mod = 1.0;
	
	for (int i = 0; i < WAVE_LEN * oversample; i++) {
		float modulation1 = linterpf(modulator_tmp, fmod(i * ceilf(index), WAVE_LEN * oversample + 1));
		float modulation2 = linterpf(modulator_tmp, fmod(i * ceilf(index + 1.0), WAVE_LEN * oversample + 1));
		float phase = (float) i / WAVE_LEN / oversample;
		carrier_tmp[i] = crossf(
			linterpf(tmp, fmod(phase * (1 + rescalef(modulation1, -1.0, 1.0, -depth, depth)) * WAVE_LEN * oversample, (WAVE_LEN  * oversample + 1))),
			linterpf(tmp, fmod(phase * (1 + rescalef(modulation2, -1.0, 1.0, -depth, depth)) * WAVE_LEN * oversample, (WAVE_LEN  * oversample + 1))),
			index_mod);
	}
	cyclicUndersample(carrier_tmp, carrier, WAVE_LEN * oversample, oversample);
}

void Wave::saveWAV(const char *filename) {
	SF_INFO info;
	info.samplerate = 44100;
	info.channels = 1;
	info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
	SNDFILE *sf = sf_open(filename, SFM_WRITE, &info);
	if (!sf)
		return;

	sf_write_float(sf, postSamples, WAVE_LEN);

	sf_close(sf);
}

void Wave::loadWAV(const char *filename) {
	clear();

	SF_INFO info;
	SNDFILE *sf = sf_open(filename, SFM_READ, &info);
	if (!sf)
		return;

	sf_read_float(sf, samples, WAVE_LEN);
	commitSamples();

	sf_close(sf);
}

void Wave::clipboardCopy() {
	memcpy(&clipboardWave, this, sizeof(*this));
	clipboardActive = true;
}

void Wave::clipboardPaste() {
	if (clipboardActive) {
		copy(&clipboardWave);
	}
}

void Wave::copy(Wave *dst) {
	memcpy(this, dst, sizeof(*this));
}
