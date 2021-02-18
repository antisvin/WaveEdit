#include "WaveEdit.hpp"
#include <string.h>
#include <sndfile.h>

#ifdef WAVETABLE_FORMAT_BLOFELD
#include <libgen.h>
#include <numeric>
#include "MidiFile.h"
#include "MidiEvent.h"
using namespace smf;
#endif


const char *crossmodNames[CROSSMOD_LEN] {
	"Modulator Rotation",
	"Phase Modulation",
	"Frequency Modulation",
	"Ring Modulation",
	"Amplitude Modulation",
	"Spectral Transfer",
	"Modulator Mix"
};


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
	
	float phase = 0.0;
	float step = 1.0 / WAVE_LEN / oversample;
	for (int i = 0; i < WAVE_LEN * oversample; i++) {
		float modulation1 = linterpf(modulator_tmp, wrap(i * ceilf(index), WAVE_LEN * oversample + 1)) * depth;
		float modulation2 = linterpf(modulator_tmp, wrap(i * ceilf(index + 1.0), WAVE_LEN * oversample + 1)) * depth;
		carrier_tmp[i] = crossf(
			linterpf(tmp, wrap((phase + modulation1) * WAVE_LEN * oversample, WAVE_LEN * oversample)),
			linterpf(tmp, wrap((phase + modulation2) * WAVE_LEN * oversample, WAVE_LEN * oversample)),
			index_mod);
		phase += step;
		phase = wrap(phase, 1.0);
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
	
	// Remove DC offset - now our FM will be sweet like yo mama
	float dc_offset = 0.0;
	for (int i = 0; i < WAVE_LEN; i++) {
		dc_offset += modulator[i];
	}
	dc_offset /= WAVE_LEN;
	for (int i = 0; i < WAVE_LEN * oversample; i++) {
		modulator_tmp[i] -= dc_offset;
	};
	
	memcpy(tmp, carrier_tmp, sizeof(float) * WAVE_LEN * oversample);
	tmp[WAVE_LEN * oversample] = tmp[0];
	
	float index_mod = fmod(index, 1.0);
	if (index_mod == 0.0) index_mod = 1.0;
	
	float phase1 = 0.0;
	float phase2 = 0.0;
	float step = 1.0 / WAVE_LEN / oversample;
	for (int i = 0; i < WAVE_LEN * oversample; i++) {
		float modulation1 = linterpf(modulator_tmp, wrap(i * ceilf(index), WAVE_LEN * oversample + 1)) * depth;
		float modulation2 = linterpf(modulator_tmp, wrap(i * ceilf(index + 1.0), WAVE_LEN * oversample + 1)) * depth;
		carrier_tmp[i] = crossf(
			linterpf(tmp, phase1 * WAVE_LEN * oversample),
			linterpf(tmp, phase2 * WAVE_LEN * oversample),
			index_mod);
		phase1 += step + modulation1 / WAVE_LEN;
		phase1 = wrap(phase1, 1.0);
		phase2 += step + modulation2 / WAVE_LEN;
		phase2 = wrap(phase2, 1.0);
	};
	cyclicUndersample(carrier_tmp, carrier, WAVE_LEN * oversample, oversample);
}


void convolution(float *carrier, const float *modulator, float depth) {
	// Build the kernel in Fourier space
	float fft[WAVE_LEN];
	float kernel[WAVE_LEN];
	float tmp[WAVE_LEN];
	memcpy(tmp, carrier, sizeof(float) * WAVE_LEN);
	
	RFFT(modulator, kernel, WAVE_LEN);
	RFFT(carrier, fft, WAVE_LEN);
	for (int k = 0; k < WAVE_LEN / 2; k++) {
		cmultf(&fft[2 * k], &fft[2 * k + 1], fft[2 * k], fft[2 * k + 1], kernel[2 * k], kernel[2 * k + 1]);
	}
	IRFFT(fft, carrier, WAVE_LEN);
	
	if (depth < 1.0) {
		for (int i = 0; i < WAVE_LEN; i++)
			carrier[i] = crossf(tmp[i], carrier[i], depth);
	}
}

void Bank::updateCrossmod() {
	float tmp_mod[WAVE_LEN];
	float out[WAVE_LEN];

	float tmp[WAVE_LEN];
	memcpy(out, carrier_wave.samples, sizeof(float) * WAVE_LEN);
    
	if (crossmod[MODULATOR_ROTATION] > 0.0) {
		RFFT(modulator_wave.samples, tmp, WAVE_LEN);
		for (int k = 0; k < WAVE_LEN / 2; k++) {
			float phase = clampf(crossmod[MODULATOR_ROTATION], 0.0, 1.0);
			float br = cosf(2 * M_PI * phase);
			float bi = -sinf(2 * M_PI * phase);
			cmultf(&tmp[2 * k], &tmp[2 * k + 1], tmp[2 * k], tmp[2 * k + 1], br, bi);
		}
		IRFFT(tmp, tmp_mod, WAVE_LEN);
	}
		else {
			memcpy(tmp_mod, modulator_wave.samples, sizeof(float) * WAVE_LEN);
		};


	// Phase Modulation
	if (crossmod[PHASE_MODULATION] > 0.0) {
		phaseModulation(out, tmp_mod, 0.0, clampf(crossmod[PHASE_MODULATION], 0.0, 1.0));
	}
	
	// Frequency Modulation
	if (crossmod[FREQUENCY_MODULATION] > 0.0) {
		frequencyModulation(out, tmp_mod, 0.0, clampf(crossmod[FREQUENCY_MODULATION], 0.0, 1.0));
	}
	
	// Ring Modulation
	if (crossmod[RING_MODULATION] > 0.0) {
		ringModulation(out, tmp_mod, 0.0, clampf(crossmod[RING_MODULATION], 0.0, 1.0));
	}
	
	// Amplitude Modulation
	if (crossmod[AMPLITUDE_MODULATION] > 0.0) {
		amplitudeModulation(out, tmp_mod, 0.0, clampf(crossmod[AMPLITUDE_MODULATION], 0.0, 1.0));
	}	
	
	// Spectral Transfer
	if (crossmod[SPECTRAL_TRANSFER] > 0.0) {
		convolution(out, tmp_mod, clampf(crossmod[SPECTRAL_TRANSFER], 0.0, 1.0));
	}
	
	// Modulator Mix
	if (crossmod[MODULATOR_MIX] > 0.0) {
		for (int i = 0; i < WAVE_LEN; i++) {
			out[i] += tmp_mod[i] * crossmod[MODULATOR_MIX];
		}
	}

	normalize_array(out, WAVE_LEN, -1.0, 1.0, 0.0);
	
	// Convert wave to spectrum
	RFFT(out, tmp, WAVE_LEN);
	// Convert spectrum to harmonics
	for (int i = 0; i < WAVE_LEN / 2; i++) {
		harmonics[i] = hypotf(tmp[2 * i], tmp[2 * i + 1]) * 2.0;
		phases[i] = atan2f(tmp[2 * i], tmp[2 * i + 1]);
	}
	IRFFT(tmp, samples, WAVE_LEN);
    
	//memcpy(samples, out, sizeof(float) * WAVE_LEN);
	for (int i = 0; i < BANK_LEN; i++) {
		memcpy(waves[i].samples, samples, sizeof(float) * WAVE_LEN);
		waves[i].commitSamples();
	}
}


void Bank::clear() {
	*this = Bank();
	modulator_wave.clear();
	carrier_wave.clear();

	for (int i = 0; i < BANK_LEN; i++) {
        waves[i].normalize = true;
		waves[i].commitSamples();
	}
}


void Bank::swap(int i, int j) {
	Wave tmp = waves[i];
	waves[i] = waves[j];
	waves[j] = tmp;
}


void Bank::randomize() {
	for (int i = 0; i < BANK_LEN; i++) {
		waves[i].randomizeEffects();
	}
}

void Bank::morph() {
	for (int i = 0; i < BANK_GRID_WIDTH; i++) {
		for (int j = 1; j < BANK_GRID_HEIGHT; j++) {
			waves[i * BANK_GRID_WIDTH + j].morphAllEffects(&waves[i * BANK_GRID_WIDTH], &waves[(i + 1) * BANK_GRID_WIDTH % BANK_LEN], ((float) j) / BANK_GRID_WIDTH);
		}
	}
}

void Bank::shuffle() {
	for (int j = BANK_LEN - 1; j >= 3; j--) {
		int i = rand() % j;
		swap(i, j);
	}
}


void Bank::setSamples(const float *in) {
	for (int j = 0; j < BANK_LEN; j++) {
		memcpy(waves[j].samples, &in[j * WAVE_LEN], sizeof(float) * WAVE_LEN);
		waves[j].commitSamples();
	}
}


void Bank::getPostSamples(float *out) {
	for (int j = 0; j < BANK_LEN; j++) {
		memcpy(&out[j * WAVE_LEN], waves[j].postSamples, sizeof(float) * WAVE_LEN);
	}
}


void Bank::save(const char *filename) {
	FILE *f = fopen(filename, "wb");
	if (!f)
		return;
	fwrite(this, sizeof(*this), 1, f);
	fclose(f);
}


void Bank::load(const char *filename) {
	clear();

	FILE *f = fopen(filename, "rb");
	if (!f)
		return;
	int ignored __attribute__((unused));
	ignored = fread(this, sizeof(*this), 1, f);
	fclose(f);

	for (int j = 0; j < BANK_LEN; j++) {
		waves[j].commitSamples();
	}
}


void Bank::saveWAV(const char *filename) {
	SF_INFO info;
	info.samplerate = 44100;
	info.channels = 1;
	info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16 | SF_ENDIAN_LITTLE;
	SNDFILE *sf = sf_open(filename, SFM_WRITE, &info);
	if (!sf)
		return;

	for (int j = 0; j < BANK_LEN; j++) {
		sf_write_float(sf, waves[j].postSamples, WAVE_LEN);
	}

	sf_close(sf);
}


void Bank::loadWAV(const char *filename) {
	clear();

	SF_INFO info;
	SNDFILE *sf = sf_open(filename, SFM_READ, &info);
	if (!sf)
		return;

	for (int i = 0; i < BANK_LEN; i++) {
		sf_read_float(sf, waves[i].samples, WAVE_LEN);
		waves[i].commitSamples();
	}

	sf_close(sf);
}


void Bank::saveWaves(const char *dirname) {
	for (int b = 0; b < BANK_LEN; b++) {
		char filename[1024];
		snprintf(filename, sizeof(filename), "%s/%02d.wav", dirname, b);

		waves[b].saveWAV(filename);
	}
}

#if WAVETABLE_FORMAT_PHMK2
void Bank::saveROM(const char *filename) {
	FILE *f = fopen(filename, "w");

	if (!f)
        	return;
	
	char line[43];
	char byte[4];
	char index[5];
	int checksum;
	char checksum_str[3];
	
	for (int i = 0; i < BANK_LEN; i++) {
		for (int j = 0; j < WAVE_LEN / HEX_LINE_WIDTH * 2; j++) {
                        strcpy(line, "10");
			sprintf(index, "%04X", (i * 16 + j) * 16);
			strcat(line, index);
			strcat(line, "00");
			for (int k = 0; k < HEX_LINE_WIDTH / 2; k++) {
				sprintf(byte, "%02X", (uint8_t) rescalef(waves[i].postSamples[j * HEX_LINE_WIDTH / 2 + k], -1.0, 1.0, 0.0, 255.0));
				strcat(line, byte);
			};
			checksum = 0;
			
			for (size_t l = 0; l < strlen(line) / 2; l++) {
				strncpy(byte, line + l * 2, 2);
				checksum += strtol(byte, NULL, 16);
				checksum &= 0xFF;
			};
			checksum = (0x100 - checksum) & 0xFF;
			sprintf(checksum_str, "%02X", checksum);
			strcat(line, checksum_str);
			fprintf(f, ":%s\n", line);
		};
	}

	fprintf(f, ":00000001FF\n");
	fclose(f);
}


void Bank::loadROM(const char *filename) {
	FILE *f = fopen(filename, "r");

	if (!f)
        	return;

	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	int data;
	char byte[2];
	
	clear();

	for (int i = 0; i < BANK_LEN; i++) {
		for (int j = 0; j < WAVE_LEN / HEX_LINE_WIDTH * 2; j++) {
			while ((getline(&line, &len, f)) != -1) {
				if ((line[7] != '0') || (line[8] != '0'))
					continue;
				for (int k = 0; k < HEX_LINE_WIDTH / 2; k+= 1) {
					byte[0] = line[k * 2 + 9];
					byte[1] = line[k * 2 + 10];
					waves[i].samples[j * HEX_LINE_WIDTH / 2 + k] = rescalef(float(strtol(byte, NULL, 16)), 0.0, 255.0, -1.0, 1.0);
				};
				break;
			}
		};
		waves[i].commitSamples();
	}

	fclose(f);
	if (line)
		free(line);
}
#endif

#if WAVETABLE_FORMAT_BLOFELD
void Bank::saveBlofeldWavetable(const char *filename){
	MidiFile outfile(filename);
	outfile.clear();
	outfile.setTicksPerQuarterNote(100);
	
	int slot = 0;
	char *fn = strdup(filename);
	char *base = basename(fn);
	free(fn);
	char name[15] = "              ";
	if (strlen(base) > 1) {
		int first_char = 0;
		if (base[0] >= '0' && base[0] <= '9' && base[1] >= '0' && base[1] <= '9') {
			char buf[3];
			strncpy(buf, base, 2);
			buf[2] = '\0';
			slot = atoi(buf);
			slot = mini(38, slot);
			first_char = 2;
		};
		for (int i = first_char; i < mini(first_char + 14, strlen(base)); i++){
			if (base[i] == '.')
				break;
			name[i - first_char] = base[i];
		}
	}
	else {
		strcpy(name, "Untitled      ");
	};
	
	for (int wave = 0; wave < BANK_LEN; wave++) {
		std::vector<uchar> mm(410);
		mm[0] = 0xf0; // SysEx
		mm[1] = 0x3e; // Waldorf ID
		mm[2] = 0x13; // Blofeld ID
		mm[3] = 0x00; // Device ID
		mm[4] = 0x12; // Wavetable Dump
		mm[5] = 0x50 + slot; // Wavetable Number
		mm[6] = wave & 0x7f; // Wave Number
		mm[7] = 0x00; // Format
		printf("%x %x %x %x %x %x %x %x\n", mm[0], mm[1], mm[2], mm[3], mm[4], mm[5], mm[6], mm[7]);
		//f0 3e 13 0 12 50 11 0

		// actual samples
		for (int i = 0; i < WAVE_LEN; i++){
			int32_t sample = (int32_t) ((clampf(waves[wave].postSamples[i], -1.0, 1.0)) * 1048575.0);
			mm[8  + 3 * i] = (sample >> 14) & 0x7f;
			mm[9  + 3 * i] = (sample >>  7) & 0x7f;
			mm[10 + 3 * i] = (sample      ) & 0x7f;
		}

		// wavetable name
		for (int i = 0; i < 14; i++){
			mm[392 + i] = name[i] & 0x7f;
		}

		mm[406] = 0x0; // Reserved
		mm[407] = 0x0; // Reserved

		int checksum = std::accumulate(mm.begin() + 7, mm.begin() + 407, 0);
		printf("%i\n", checksum);
		mm[408] = checksum & 0x7f;
		mm[409] = 0xf7; // End

		outfile.addEvent(0, wave, mm);
	}

	outfile.write(filename);
	
};

void Bank::loadBlofeldWavetable(const char *filename){
	MidiFile infile(filename);
	float samples[WAVE_LEN] = {};
	int wave = 0;
	for (int i = 0; i < infile.getEventCount(0) && wave < BANK_LEN; i++) {
		MidiEvent event = infile.getEvent(0, i);
		printf("%x %x %x %x %x %x %x %x\n", event[0], event[1], event[2], event[3], event[4], event[5], event[6], event[7]);
		if (event[0] != 0xf0 || event[1] != 0x3e || event[2] != 0x13 || event[3] != 0x00 || event[4] != 0x12)
			continue;
		for (int j = 0; j < WAVE_LEN; j++){
			int32_t sample = (
				((int32_t)(event[8  + 3 * j]) << 14) |
				((int32_t)(event[9  + 3 * j]) << 7) |
				((int32_t)(event[10 + 3 * j]))
			);
			if (sample &  0x00100000) 
				sample |= 0xfff00000;
			waves[wave].samples[j] = clampf(((float) sample) / 1048575.0, -1.0, 1.0);
			if (wave == 0) printf("%i %x,%x,%x %i %f\n", j, event[8 + 3 * j], event[9 + 3 * j], event[10 + 3 * j], sample, waves[wave].samples[j]);
		}
		
		int checksum = std::accumulate(event.begin() + 7, event.begin() + 407, 0);
		waves[wave].commitSamples();
		wave++;
	}
};
#endif
