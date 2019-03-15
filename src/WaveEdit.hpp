#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#define _USE_MATH_DEFINES
#include <math.h>

#include <string.h>
#include <thread>
#include <vector>
#include <complex>


#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)


////////////////////
// math.cpp
////////////////////

// Integers

inline int eucmodi(int a, int base) {
	int mod = a % base;
	return mod < 0 ? mod + base : mod;
}

inline int mini(int a, int b) {
	return a < b ? a : b;
}

inline int maxi(int a, int b) {
	return a > b ? a : b;
}

/** Limits a value between a minimum and maximum */
inline int clampi(int x, int min, int max) {
	return x > max ? max : x < min ? min : x;
}

// Floats

inline float sgnf(float x) {
	return copysignf(1.0, x);
}

/** Limits a value between a minimum and maximum */
inline float clampf(float x, float min, float max) {
	return x > max ? max : x < min ? min : x;
}


/** If the magnitude of x if less than eps, return 0 */
inline float chopf(float x, float eps) {
	return (-eps < x && x < eps) ? 0.0 : x;
}

inline float rescalef(float x, float xMin, float xMax, float yMin, float yMax) {
	return yMin + (x - xMin) / (xMax - xMin) * (yMax - yMin);
}

inline float crossf(float a, float b, float frac) {
	return (1.0 - frac) * a + frac * b;
}

// Bandlimiting

inline float blep(float t, float dt) {
	if (t < dt) {
		return -(t / dt - 1) * (t / dt - 1);
	} else if (t > 1 - dt) {
		return ((t - 1) / dt + 1) * ((t - 1) / dt + 1);
	} else {
		return 0;
	}
}

inline float blamp(float t, float dt) {
	if (t < dt) {
		t = t / dt - 1;
		return -1 / 3.0 * t * t * t;
	} else if (t > 1 - dt) {
		t = (t - 1) / dt + 1;
		return 1 / 3.0 * t * t * t;
	} else {
		return 0;
	}
}


/** Linearly interpolate an array `p` with index `x`
Assumes that the array at `p` is of length at least ceil(x).
*/
inline float linterpf(const float *p, float x) {
	int xi = x;
	float xf = x - xi;
	if (xf > 0.0)
		return crossf(p[xi], p[xi + 1], xf);
	else
		return p[xi];
	/*
	if (xf < 1e-6)
		return p[xi];
	else
		return crossf(p[xi], p[xi + 1], xf);
	*/
}


inline void normalize_array(float *data, int size, float new_min, float new_max, float empty) {
	float max = -INFINITY;
	float min = INFINITY;
	for (int i = 0; i < size; i++) {
		if (data[i] > max) max = data[i];
		if (data[i] < min) min = data[i];
	}

	if (max - min >= 1e-6) {
		for (int i = 0; i < size; i++) {
			data[i] = rescalef(data[i], min, max, new_min, new_max);
		}
	}
	else {
		memset(data, empty, sizeof(float) * size);
	}
}

/** Returns a random number on [0, 1) */
inline float randf() {
	return (float)rand() / RAND_MAX;
}

/** Complex multiply c = a * b
It is of course acceptable to reuse arguments
i.e. cmultf(&ar, &ai, ar, ai, br, bi)
*/
inline void cmultf(float *cr, float *ci, float ar, float ai, float br, float bi) {
	*cr = ar * br - ai * bi;
	*ci = ar * bi + ai * br;
}

void RFFT(const float *in, float *out, int len);
void IRFFT(const float *in, float *out, int len);

int resample(const float *in, int inLen, float *out, int outLen, double ratio);
void cyclicOversample(const float *in, float *out, int len, int oversample);
void cyclicUndersample(const float *in, float *out, int len, int undersample);
void i16_to_f32(const int16_t *in, float *out, int length);
void f32_to_i16(const float *in, int16_t *out, int length);


////////////////////
// util.cpp
////////////////////

/** Opens a URL, also happens to work with PDFs */
void openBrowser(const char *url);
/** Caller must free(). Returns NULL if unsuccessful */
float *loadAudio(const char *filename, int *length);
/** Converts a printf format to a std::string */
std::string stringf(const char *format, ...);
/** Truncates a string if needed, inserting ellipses (...), to be no greater than `maxLen` characters */
void ellipsize(char *str, int maxLen);
unsigned char *base64_encode(const unsigned char *src, size_t len, size_t *out_len);
unsigned char *base64_decode(const unsigned char *src, size_t len, size_t *out_len);


////////////////////
// wave.cpp
////////////////////

#ifdef WAVETABLE_FORMAT_BLOFELD
#define WAVE_LEN 128
#else
#define WAVE_LEN 256
#endif


enum EffectID {
	PHASE_MODULATION,
	FREQUENCY_MODULATION,
	RING_MODULATION,
	AMPLITUDE_MODULATION,
	PRE_GAIN,
	PHASE_SHIFT,
	HARMONIC_SHIFT,
	HARMONIC_ASYMETRY,
	HARMONIC_BALANCE,
	HARMONIC_STRETCH,
	HARMONIC_FOLD,
	PHASE_DISTORTION,
	CUBIC_DISTORTION,
	COMB,
	CHEBYSHEV,
	SAMPLE_AND_HOLD,
	TRACK_AND_HOLD,
	QUANTIZATION,
	SLEW,
	LOWPASS,
	HIGHPASS,
	PHASE_FEEDBACK,
	FREQUENCY_FEEDBACK,
	RING_FEEDBACK,
	AMPLITUDE_FEEDBACK,
	MODULATION_INDEX,
	LOW_BOOST,
	MID_BOOST,
	HIGH_BOOST,
	POST_GAIN,
	EFFECTS_LEN
};

extern const char *effectNames[EFFECTS_LEN];

struct Wave {
	float samples[WAVE_LEN];
	/** FFT of wave, interleaved complex numbers */
	float spectrum[WAVE_LEN];
	/** Norm of spectrum */
	float harmonics[WAVE_LEN / 2];
	/** Wave after effects have been applied */
	float postSamples[WAVE_LEN];
	float postSpectrum[WAVE_LEN];
	float postHarmonics[WAVE_LEN / 2];

	float effects[EFFECTS_LEN];
	bool cycle;
	bool normalize;

	void clear();
	/** Generates post arrays from the sample array, by applying effects */
	void updatePost();
	void commitSamples();
	void commitHarmonics();
	void clearEffects();
	void morphEffect(Wave *from_wave, Wave *to_wave, EffectID effect, float fade);
	void morphAllEffects(Wave *from_wave, Wave *to_wave, float fade);
	void applyRingModulation();
	void applyAmplitudeModulation();
	void applyPhaseModulation();
	void applyFrequencyModulation();
	void applySpectralTransfer();
	/** Applies effects to the sample array and resets the effect parameters */
	void bakeEffects();
	void randomizeEffects();
	void saveWAV(const char *filename);
	void loadWAV(const char *filename);
	/** Writes to a global state */
	void copy(Wave *dst);
	void clipboardCopy();
	void clipboardPaste();
};

extern bool clipboardActive;
void ringModulation(float *carrier, const float *modulator, float index, float depth);
void amplitudeModulation(float *carrier, const float *modulator, float index, float depth);
void phaseModulation(float *carrier, const float *modulator, float index, float depth);
void frequencyModulation(float *carrier, const float *modulator, float index, float depth);


////////////////////
// oscillator.cpp
////////////////////

enum WaveShapeID {
	SINE,
	HALF_SINE,
	TRIANGLE,
	TRI_PULSE,
	SQUARE,
	RECTANGLE,
	TRAPEZOID,
	WAVE_SHAPES_LEN
};


struct Oscillator {
	float pulse_width;
	float dt;
	
	void render(WaveShapeID lower_a, WaveShapeID lower_b, float lower_ratio, WaveShapeID upper_a, WaveShapeID upper_b, float upper_ratio, float *samples);
	
	void getWave(WaveShapeID shape, bool lower, bool upper, float *samples, float *compensation);
	
	void sineWave(bool lower, bool upper, float *samples, float *compenation);
	void halfSineWave(bool lower, bool upper, float *samples, float *compenation);
	void triangleWave(bool lower, bool upper, float *samples, float *compenation);
	void triPulseWave(bool lower, bool upper, float *samples, float *compenation);
	void squareWave(bool lower, bool upper, float *samples, float *compenation);
	void rectangleWave(bool lower, bool upper, float *samples, float *compenation);
	void trapezoidWave(bool lower, bool upper, float *samples, float *compenation);
};



//extern Oscillator osc_a, osc_b;



////////////////////
// basewave.cpp
////////////////////


#define WAVE_SHAPES_LEN 7


struct BaseWave {
	float lower_shape, upper_shape;
	
	/*
	float lower_shape_a, lower_shape_b, upper_shape_a, upper_shape_b;
	float lower_crossfade, upper_crossfade, lower_mult, upper_mult;
	float lower_pulse_width, upper_pulse_width;
	*/
	
	bool lock_shapes;
	float pulse_width, brightness;
	float bottom_angle, bottom_magnitude;
	float bottom_x, bottom_y;
	float top_angle, top_magnitude;
	float top_x, top_y;
	float bezier_ratio, bezier_weight;
	float resonance;
	
	float samples[WAVE_LEN];
	float shape[WAVE_LEN];
	float phasor[WAVE_LEN];
	
	void clear();
	void updateShape();
	void updatePhasor();
	void generateSamples(bool update_waves);
	void updateSamples(bool update_waves);
	
	void generateShape(const float *shape_phasor, float *samples);
	float harmonics[WAVE_LEN / 2];
};


////////////////////
// bank.cpp
////////////////////
#if WAVETABLE_FORMAT_WAVEEDIT
#define BANK_LEN 64
#define BANK_GRID_WIDTH 8
#define BANK_GRID_HEIGHT 8

#elif WAVETABLE_FORMAT_PHMK2
#define BANK_LEN 256
#define BANK_GRID_WIDTH 16
#define BANK_GRID_HEIGHT 16
#define HEX_LINE_WIDTH 32
// This is width of actual data per HEX file line, currently hardcoded to avoid proper file parsing

#elif WAVETABLE_FORMAT_BLOFELD
#define BANK_LEN 64
#define BANK_GRID_WIDTH 8
#define BANK_GRID_HEIGHT 8

#endif

struct Bank {
	Wave waves[BANK_LEN];
	BaseWave carrier_wave;
	BaseWave modulator_wave;

	void clear();
	void swap(int i, int j);
	void shuffle();
	/** `in` must be length BANK_LEN * WAVE_LEN */
	void setSamples(const float *in);
	void getPostSamples(float *out);
	void duplicateToAll(int waveId);
	/** Binary dump of the bank struct */
	void save(const char *filename);
	void load(const char *filename);
	/** WAV file with BANK_LEN * WAVE_LEN samples */
	void saveWAV(const char *filename);
	void loadWAV(const char *filename);
	/** Saves each wave to its own file in a directory */
	void saveWaves(const char *dirname);
#if WAVETABLE_FORMAT_PHMK2
	/** ROM file with BANK_LEN * WAVE_LEN samples */
	void saveROM(const char *filename);
	void loadROM(const char *filename);
#endif
#if WAVETABLE_FORMAT_BLOFELD
	void saveBlofeldWavetable(const char *filename);
	void loadBlofeldWavetable(const char *filename);
#endif
};


////////////////////
// history.cpp
////////////////////

/** Call as much as you like. History will only be pushed if a time delay between the last call has occurred. */
void historyPush();
void historyUndo();
void historyRedo();
void historyClear();

extern Bank currentBank;


////////////////////
// catalog.cpp
////////////////////

struct CatalogFile {
	float samples[WAVE_LEN];
	std::string name;
};

struct CatalogCategory {
	std::vector<CatalogFile> files;
	std::string name;
};

extern std::vector<CatalogCategory> catalogCategories;

void catalogInit();


////////////////////
// audio.cpp
////////////////////

// TODO Some of these should not be exposed in the header
extern float playVolume;
extern float playFrequency;
extern float playFrequencySmooth;
extern bool playEnabled;
extern bool playModeXY;
extern bool morphInterpolate;
extern float morphX;
extern float morphY;
extern float morphZ;
extern float morphZSpeed;
extern int playIndex;
extern const char *audioDeviceName;
extern Bank *playingBank;

int audioGetDeviceCount();
const char *audioGetDeviceName(int deviceId);
void audioClose();
void audioOpen(int deviceId);
void audioInit();
void audioDestroy();


////////////////////
// widgets.cpp
////////////////////

enum Tool {
	NO_TOOL,
	PENCIL_TOOL,
	BRUSH_TOOL,
	GRAB_TOOL,
	LINE_TOOL,
	ERASER_TOOL,
	SMOOTH_TOOL,
	NUM_TOOLS
};


bool renderWave(const char *name, float height, float *points, int pointsLen, const float *lines, int linesLen, enum Tool tool = NO_TOOL);
bool renderPhasor(const char *name, float height, float *points, int pointsLen, const float *lines, int linesLen, enum Tool tool, float bottom_x, float bottom_y, float bottom_magnitude, float top_x, float top_y, float top_magnitude);
bool renderHistogram(const char *name, float height, float *bars, int barsLen, const float *ghost, int ghostLen, enum Tool tool);
void renderBankGrid(const char *name, float height, int gridWidth, float *gridX, float *gridY);
void renderWaterfall(const char *name, float height, float amplitude, float angle, float *activeZ);
/** A widget like renderWave() except without editing, and bank lines are overlaid
Returns the relative amount dragged
*/
float renderBankWave(const char *name, float height, const float *lines, int linesLen, float bankStart, float bankEnd, int bankLen);

////////////////////
// ui.cpp
////////////////////

void renderWaveMenu();
void uiInit();
void uiDestroy();
void uiRender();

// Selections span the range between these indices
extern int selectedId;
extern int lastSelectedId;
extern char lastFilename[1024];


////////////////////
// db.cpp
////////////////////

void dbInit();
void dbPage();


////////////////////
// import.cpp
////////////////////

void importPage();
