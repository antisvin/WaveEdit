#include "WaveEdit.hpp"
#include <string.h>
#include <sndfile.h>


void Bank::clear() {
	// The lazy way
	memset(this, 0, sizeof(Bank));

	for (int i = 0; i < BANK_LEN; i++) {
		waves[i].commitSamples();
	}
}


void Bank::swap(int i, int j) {
	Wave tmp = waves[i];
	waves[i] = waves[j];
	waves[j] = tmp;
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


void Bank::duplicateToAll(int waveId) {
	for (int j = 0; j < BANK_LEN; j++) {
		if (j != waveId)
			waves[j] = waves[waveId];
		// No need to commit the wave because we're copying everything
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
	fread(this, sizeof(*this), 1, f);
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
				sprintf(byte, "%02X", (uint8_t) rescalef(waves[i].samples[j * HEX_LINE_WIDTH / 2 + k], -1.0, 1.0, 0.0, 255.0));
				strcat(line, byte);
			};
			checksum = 0;
			
			for (size_t l = 0; l < strlen(line) / 2; l++) {
				strncpy(byte, line + l * 2, 2);
				checksum += strtol(byte, NULL, 16);
				checksum &= 0xFF;
			};
			checksum = 0xFF - checksum;
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


