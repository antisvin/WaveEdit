#include "WaveEdit.hpp"

#ifdef ARCH_LIN
	#include <linux/limits.h>
#endif
#ifdef ARCH_MAC
	#include <sys/syslimits.h>
#endif
#include <libgen.h>

#include <SDL.h>
#include <SDL_opengl.h>

#include "imgui.h"
#include "imgui_internal.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include "osdialog/osdialog.h"

#include "lodepng/lodepng.h"

#include "tablabels.hpp"


static bool showTestWindow = false;
static ImTextureID logoTextureLight;
static ImTextureID logoTextureDark;
static ImTextureID logoTexture;
char lastFilename[1024] = "";
static int styleId = 0;
int selectedId = 0;
int lastSelectedId = 0;


static void refreshStyle();


enum Page {
	CARRIER_WAVE_PAGE=0,
	MODULATOR_WAVE_PAGE,
	EDITOR_PAGE,
	EFFECT_PAGE,
	GRID_PAGE,
	WATERFALL_PAGE,
	IMPORT_PAGE,
	DB_PAGE,
	NUM_PAGES
};

Page currentPage = CARRIER_WAVE_PAGE;


static ImVec4 lighten(ImVec4 col, float p) {
	col.x = crossf(col.x, 1.0, p);
	col.y = crossf(col.y, 1.0, p);
	col.z = crossf(col.z, 1.0, p);
	col.w = crossf(col.w, 1.0, p);
	return col;
}

static ImVec4 darken(ImVec4 col, float p) {
	col.x = crossf(col.x, 0.0, p);
	col.y = crossf(col.y, 0.0, p);
	col.z = crossf(col.z, 0.0, p);
	col.w = crossf(col.w, 0.0, p);
	return col;
}

static ImVec4 alpha(ImVec4 col, float a) {
	col.w *= a;
	return col;
}


static ImTextureID loadImage(const char *filename) {
	GLuint textureId;
	glGenTextures(1, &textureId);
	glBindTexture(GL_TEXTURE_2D, textureId);
	unsigned char *pixels;
	unsigned int width, height;
	unsigned err = lodepng_decode_file(&pixels, &width, &height, filename, LCT_RGBA, 8);
	assert(!err);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
	return (void*)(intptr_t) textureId;
}


static void getImageSize(ImTextureID id, int *width, int *height) {
	GLuint textureId = (GLuint)(intptr_t) id;
	glBindTexture(GL_TEXTURE_2D, textureId);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, height);
	glBindTexture(GL_TEXTURE_2D, 0);
}


static void selectWave(int waveId) {
	selectedId = waveId;
	lastSelectedId = selectedId;
	morphX = (float)(selectedId % BANK_GRID_WIDTH);
	morphY = (float)(selectedId / BANK_GRID_WIDTH);
	morphZ = (float)selectedId;
}


static void refreshMorphSnap() {
	if (!morphInterpolate && morphZSpeed <= 0.f) {
		morphX = roundf(morphX);
		morphY = roundf(morphY);
		morphZ = roundf(morphZ);
	}
}

/** Focuses to a page which displays the current bank, useful when loading a new bank and showing the user some visual feedback that the bank has changed. */
static void showCurrentBankPage() {
	switch (currentPage) {
		case EFFECT_PAGE:
		case IMPORT_PAGE:
		case DB_PAGE:
			currentPage = EDITOR_PAGE;
			break;
		default:
			break;
	}
}

static void menuManual() {
	openBrowser("manual.pdf");
}

static void menuWebsite() {
	openBrowser("http://synthtech.com/waveedit");
}

static void menuNewBank() {
	showCurrentBankPage();
	currentBank.clear();
	lastFilename[0] = '\0';
	historyPush();
}

/** Caller must free() return value, guaranteed to not be NULL */
static char *getLastDir() {
	if (lastFilename[0] == '\0') {
		return strdup(".");
	}
	else {
		char filename[PATH_MAX];
		strncpy(filename, lastFilename, sizeof(filename));
		char *dir = dirname(filename);
		return strdup(dir);
	}
}

static void menuOpenBank() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
	if (path) {
		showCurrentBankPage();
		currentBank.loadWAV(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		historyPush();
		free(path);
	}
	free(dir);
}

static void menuSaveBankAs() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.wav", NULL);
	if (path) {
		currentBank.saveWAV(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		free(path);
	}
	free(dir);
}

static void menuSaveBank() {
	if (lastFilename[0] != '\0')
		currentBank.saveWAV(lastFilename);
	else
		menuSaveBankAs();
}

static void menuSaveWaves() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN_DIR, dir, NULL, NULL);
	if (path) {
		currentBank.saveWaves(path);
		free(path);
	}
	free(dir);
}

#if WAVETABLE_FORMAT_PHMK2
static void menuOpenRom() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
	if (path) {
		showCurrentBankPage();
		currentBank.loadROM(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		historyPush();
		free(path);
	}
	free(dir);
}

static void menuSaveRomAs() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.hex", NULL);
	if (path) {
		currentBank.saveROM(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		free(path);
	}
	free(dir);
}

static void menuSaveRom() {
	if (lastFilename[0] != '\0')
		currentBank.saveROM(lastFilename);
	else
		menuSaveRomAs();
}
#endif

#if WAVETABLE_FORMAT_BLOFELD
static void menuOpenBlofeldWavetable() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
	if (path) {
		showCurrentBankPage();
		currentBank.loadBlofeldWavetable(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		historyPush();
		free(path);
	}
	free(dir);
}

static void menuSaveBlofeldWavetableAs() {
	char *dir = getLastDir();
	char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.mid", NULL);
	if (path) {
		currentBank.saveBlofeldWavetable(path);
		snprintf(lastFilename, sizeof(lastFilename), "%s", path);
		free(path);
	}
	free(dir);
}

static void menuSaveBlofeldWavetable() {
	if (lastFilename[0] != '\0')
		currentBank.saveBlofeldWavetable(lastFilename);
	else
		menuSaveBlofeldWavetableAs();
}
#endif

static void menuQuit() {
	SDL_Event event;
	event.type = SDL_QUIT;
	SDL_PushEvent(&event);
}

static void menuSelectAll() {
	selectedId = 0;
	lastSelectedId = BANK_LEN-1;
}

static void menuCopy() {
	currentBank.waves[selectedId].clipboardCopy();
}

static void menuCut() {
	currentBank.waves[selectedId].clipboardCopy();
	currentBank.waves[selectedId].clear();
	historyPush();
}

static void menuPaste() {
	currentBank.waves[selectedId].clipboardPaste();
	historyPush();
}

static void menuDuplicateToRow() {
	int pasteStart = selectedId / BANK_GRID_WIDTH * BANK_GRID_WIDTH;
	for (int i = pasteStart; i < pasteStart + BANK_GRID_WIDTH; i++){
		if (i != selectedId)
			currentBank.waves[i].copy(&currentBank.waves[selectedId]);
	}
	historyPush();
}


static void menuClear() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].clear();
	}
	historyPush();
}

static void menuRandomize() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].randomizeEffects();
	}
	historyPush();
}

static void menuPasteSelected() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].clipboardPaste();
	}
	historyPush();
}

static void menuWavesAM() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].applyAmplitudeModulation();
	}
	historyPush();
}

static void menuWavesRM() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].applyRingModulation();
	}
	historyPush();
}

static void menuWavesPM() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].applyPhaseModulation();
	}
	historyPush();
}

static void menuWavesFM() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].applyFrequencyModulation();
	}
	historyPush();
}

static void menuWavesST() {
	for (int i = mini(selectedId, lastSelectedId); i <= maxi(selectedId, lastSelectedId); i++) {
		currentBank.waves[i].applySpectralTransfer();
	}
	historyPush();
}

static void menuMorphEffectsAll() {
	int a = mini(selectedId, lastSelectedId);
	int b = maxi(selectedId, lastSelectedId);
	Wave *wave_a = &currentBank.waves[a];
	Wave *wave_b = &currentBank.waves[b];
	for (int i = a + 1; i < b; i++) {
		currentBank.waves[i].morphAllEffects(wave_a, wave_b, (float)(i - a) / (float)(b - a));
	}
	historyPush();
}

static void menuMorphEffect(EffectID effect) {
	int a = mini(selectedId, lastSelectedId);
	int b = maxi(selectedId, lastSelectedId);
	Wave *wave_a = &currentBank.waves[a];
	Wave *wave_b = &currentBank.waves[b];
	for (int i = a + 1; i < b; i++) {
		currentBank.waves[i].morphEffect(wave_a, wave_b, effect, (float)(i - a) / (float)(b - a));
	}
	historyPush();
}

static void incrementSelectedId(int delta) {
	selectWave(clampi(selectedId + delta, 0, BANK_LEN-1));
}

static void menuKeyCommands() {
	ImGuiContext &g = *GImGui;
	ImGuiIO &io = ImGui::GetIO();

	if (io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl) {
		if (ImGui::IsKeyPressed(SDL_SCANCODE_N) && !io.KeyShift && !io.KeyAlt)
			menuNewBank();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_O) && !io.KeyShift && !io.KeyAlt)
			menuOpenBank();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_S) && !io.KeyShift && !io.KeyAlt)
			menuSaveBank();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_S) && io.KeyShift && !io.KeyAlt)
			menuSaveBankAs();
		#if WAVETABLE_FORMAT_PHMK2
		if (ImGui::IsKeyPressed(SDL_SCANCODE_R) && !io.KeyShift && !io.KeyAlt)
			menuOpenRom();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_M) && !io.KeyShift && !io.KeyAlt)
			menuSaveRom();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_M) && io.KeyShift && !io.KeyAlt)
			menuSaveRomAs();
		#endif
		#if WAVETABLE_FORMAT_BLOFELD
		if (ImGui::IsKeyPressed(SDL_SCANCODE_R) && !io.KeyShift && !io.KeyAlt)
			menuOpenBlofeldWavetable();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_M) && !io.KeyShift && !io.KeyAlt)
			menuSaveBlofeldWavetable();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_M) && io.KeyShift && !io.KeyAlt)
			menuSaveBlofeldWavetableAs();
		#endif

		if (ImGui::IsKeyPressed(SDL_SCANCODE_Q) && !io.KeyShift && !io.KeyAlt)
			menuQuit();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_Z) && !io.KeyShift && !io.KeyAlt)
			historyUndo();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_Z) && io.KeyShift && !io.KeyAlt)
			historyRedo();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_A) && !io.KeyShift && !io.KeyAlt)
			menuSelectAll();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_C) && !io.KeyShift && !io.KeyAlt)
			menuCopy();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_X) && !io.KeyShift && !io.KeyAlt)
			menuCut();
		if (ImGui::IsKeyPressed(SDL_SCANCODE_D) && !io.KeyShift && !io.KeyAlt)
			menuDuplicateToRow();
		if (clipboardActive) {
			if (ImGui::IsKeyPressed(SDL_SCANCODE_V) && !io.KeyShift && !io.KeyAlt)
				menuPaste();
			if (ImGui::IsKeyPressed(SDL_SCANCODE_1) && !io.KeyShift && !io.KeyAlt)
				menuWavesAM();
			if (ImGui::IsKeyPressed(SDL_SCANCODE_2) && !io.KeyShift && !io.KeyAlt)
				menuWavesRM();
			if (ImGui::IsKeyPressed(SDL_SCANCODE_3) && !io.KeyShift && !io.KeyAlt)
				menuWavesPM();
			if (ImGui::IsKeyPressed(SDL_SCANCODE_4) && !io.KeyShift && !io.KeyAlt)
				menuWavesFM();
			if (ImGui::IsKeyPressed(SDL_SCANCODE_5) && !io.KeyShift && !io.KeyAlt)
				menuWavesST();
		}
	}
	// I have NO idea why the scancode is needed here but the keycodes are needed for the letters.
	// It looks like SDLZ_F1 is not defined correctly or something.
	if (ImGui::IsKeyPressed(SDL_SCANCODE_F1))
		menuManual();

	if (!io.KeySuper && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt) {
		// Only trigger these key commands if no text box is focused
		if (!g.ActiveId || g.ActiveId != GImGui->InputTextState.ID) {
			if (ImGui::IsKeyPressed(SDL_SCANCODE_V))
				menuPasteSelected();
			if (ImGui::IsKeyPressed(SDL_SCANCODE_R))
				menuRandomize();
			if (ImGui::IsKeyPressed(SDL_SCANCODE_A))
				menuMorphEffectsAll();
			if (ImGui::IsKeyPressed(io.ConfigMacOSXBehaviors ? SDL_SCANCODE_BACKSPACE : SDL_SCANCODE_DELETE))
				menuClear();
			// Pages
			if (ImGui::IsKeyPressed(SDL_SCANCODE_SPACE))
				playEnabled = !playEnabled;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_1))
				currentPage = CARRIER_WAVE_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_2))
				currentPage = MODULATOR_WAVE_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_3))
				currentPage = EDITOR_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_4))
				currentPage = EFFECT_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_5))
				currentPage = GRID_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_6))
				currentPage = WATERFALL_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_7))
				currentPage = IMPORT_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_8))
				currentPage = DB_PAGE;
			if (ImGui::IsKeyPressed(SDL_SCANCODE_UP))
				incrementSelectedId(currentPage == GRID_PAGE ? -BANK_GRID_WIDTH : -1);
			if (ImGui::IsKeyPressed(SDL_SCANCODE_DOWN))
				incrementSelectedId(currentPage == GRID_PAGE ? BANK_GRID_WIDTH : 1);
			if (ImGui::IsKeyPressed(SDL_SCANCODE_LEFT))
				incrementSelectedId(-1);
			if (ImGui::IsKeyPressed(SDL_SCANCODE_RIGHT))
				incrementSelectedId(1);
		}
	}
}

void renderWaveMenu() {
	char menuName[128];
	int selectedStart = mini(selectedId, lastSelectedId);
	int selectedEnd = maxi(selectedId, lastSelectedId);
	if (selectedStart != selectedEnd)
		snprintf(menuName, sizeof(menuName), "(Wave %d to %d)", selectedStart, selectedEnd);
	else
		snprintf(menuName, sizeof(menuName), "(Wave %d)", selectedId);
	ImGui::MenuItem(menuName, NULL, false, false);

	if (ImGui::MenuItem("Clear", "Delete")) {
		menuClear();
	}
	if (ImGui::MenuItem("Paste To Selected", "V")) {
		menuPasteSelected();
	}
	if (ImGui::MenuItem("Randomize Effects", "R")) {
		menuRandomize();
	}
	if (ImGui::MenuItem("Amplitude Modulation", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+1" : "Ctrl+1", false, clipboardActive)) {
		menuWavesAM();
	}
	if (ImGui::MenuItem("Ring Modulation", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+2" : "Ctrl+2", false, clipboardActive)) {
		menuWavesRM();
	}
	if (ImGui::MenuItem("Phase Modulation", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+3" : "Ctrl+3", false, clipboardActive)) {
		menuWavesPM();
	}
	if (ImGui::MenuItem("Frequency Modulation", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+4" : "Ctrl+4", false, clipboardActive)) {
		menuWavesFM();
	}
	if (ImGui::MenuItem("Spectral Transfer", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+5" : "Ctrl+5", false, clipboardActive)) {
		menuWavesST();
	}
	if (ImGui::MenuItem("Morph All Effects", "A")) {
		menuMorphEffectsAll();
	}
	char fxName[128];
	for (int effect = PRE_GAIN ; effect < EFFECTS_LEN; effect++) {
		sprintf(fxName, "Morph %s", effectNames[effect]);
		if (ImGui::MenuItem(fxName)) {
			menuMorphEffect(static_cast<EffectID>(effect));
		}
	}

	if (selectedStart != selectedEnd) {
		ImGui::MenuItem("##spacer3", NULL, false, false);
		snprintf(menuName, sizeof(menuName), "(Wave %d)", selectedId);
		ImGui::MenuItem(menuName, NULL, false, false);
	}

	if (ImGui::MenuItem("Copy", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+C" : "Ctrl+C")) {
		menuCopy();
	}
	if (ImGui::MenuItem("Cut", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+X" : "Ctrl+X")) {
		menuCut();
	}
	if (ImGui::MenuItem("Paste", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+V" : "Ctrl+V", false, clipboardActive)) {
		menuPaste();
	}
	if (ImGui::MenuItem("Duplicate To Row", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+D" : "Ctrl+D")) {
		menuDuplicateToRow();
	}

	if (ImGui::MenuItem("Open Wave...")) {
		char *dir = getLastDir();
		char *path = osdialog_file(OSDIALOG_OPEN, dir, NULL, NULL);
		if (path) {
			currentBank.waves[selectedId].loadWAV(path);
			historyPush();
			snprintf(lastFilename, sizeof(lastFilename), "%s", path);
			free(path);
		}
		free(dir);
	}
	if (ImGui::MenuItem("Save Wave As...")) {
		char *dir = getLastDir();
		char *path = osdialog_file(OSDIALOG_SAVE, dir, "Untitled.wav", NULL);
		if (path) {
			currentBank.waves[selectedId].saveWAV(path);
			snprintf(lastFilename, sizeof(lastFilename), "%s", path);
			free(path);
		}
		free(dir);
	}
}

void renderMenu() {
	menuKeyCommands();

	// HACK
	// Display a window on top of the menu with the logo, since I'm too lazy to make my own custom MenuImageItem widget
	{
		int width, height;
		getImageSize(logoTexture, &width, &height);
		ImVec2 padding = ImVec2(8, 4);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(0, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(width + 2 * padding.x, height + 2 * padding.y));
		if (ImGui::Begin("Logo", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoInputs)) {
			ImGui::Image(logoTexture, ImVec2(width, height));
			ImGui::End();
		}
		ImGui::PopStyleVar();
		ImGui::PopStyleVar();
	}

	// Draw main menu
	if (ImGui::BeginMenuBar()) {
		// This will be hidden by the window with the logo
		if (ImGui::BeginMenu("                        v" TOSTRING(VERSION), false)) {
			ImGui::EndMenu();
		}
		// File
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("New Bank", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+N" : "Ctrl+N"))
				menuNewBank();
			if (ImGui::MenuItem("Open Bank...", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+O" : "Ctrl+O"))
				menuOpenBank();
			if (ImGui::MenuItem("Save Bank", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+S" : "Ctrl+S"))
				menuSaveBank();
			if (ImGui::MenuItem("Save Bank As...", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Shift+S" : "Ctrl+Shift+S"))
				menuSaveBankAs();
			if (ImGui::MenuItem("Save Waves to Folder...", NULL))
				menuSaveWaves();
			#ifdef WAVETABLE_FORMAT_PHMK2
			if (ImGui::MenuItem("Open Rom...", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+R" : "Ctrl+R"))
				menuOpenRom();
			if (ImGui::MenuItem("Save Rom", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+M" : "Ctrl+M"))
				menuSaveRom();
			if (ImGui::MenuItem("Save Rom As...", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Shift+M" : "Ctrl+Shift+M"))
				menuSaveRomAs();
			#endif
			#ifdef WAVETABLE_FORMAT_BLOFELD
			if (ImGui::MenuItem("Open Blofeld Wavetable...", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+R" : "Ctrl+R"))
				menuOpenBlofeldWavetable();
			if (ImGui::MenuItem("Save Blofeld Wavetable", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+M" : "Ctrl+M"))
				menuSaveBlofeldWavetable();
			if (ImGui::MenuItem("Save Blofeld Wavetable As...", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Shift+M" : "Ctrl+Shift+M"))
				menuSaveBlofeldWavetableAs();
			#endif
			if (ImGui::MenuItem("Quit", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Q" : "Ctrl+Q"))
				menuQuit();

			ImGui::EndMenu();
		}
		// Edit
		if (ImGui::BeginMenu("Edit")) {
			if (ImGui::MenuItem("Undo", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Z" : "Ctrl+Z"))
				historyUndo();
			if (ImGui::MenuItem("Redo", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+Shift+Z" : "Ctrl+Shift+Z"))
				historyRedo();
			if (ImGui::MenuItem("Select All", ImGui::GetIO().ConfigMacOSXBehaviors ? "Cmd+A" : "Ctrl+A"))
				menuSelectAll();
			ImGui::MenuItem("##spacer", NULL, false, false);
			renderWaveMenu();
			ImGui::EndMenu();
		}
		// Audio Output
		if (ImGui::BeginMenu("Audio Output")) {
			int deviceCount = audioGetDeviceCount();
			for (int deviceId = 0; deviceId < deviceCount; deviceId++) {
				const char *deviceName = audioGetDeviceName(deviceId);
				if (ImGui::MenuItem(deviceName, NULL, false)) audioOpen(deviceId);
			}
			ImGui::EndMenu();
		}
		// Colors
		if (ImGui::BeginMenu("Colors")) {
			if (ImGui::MenuItem("Sol", NULL, styleId == 0)) {
				styleId = 0;
				refreshStyle();
			}
			if (ImGui::MenuItem("Mars", NULL, styleId == 1)) {
				styleId = 1;
				refreshStyle();
			}
			if (ImGui::MenuItem("Mercury", NULL, styleId == 2)) {
				styleId = 2;
				refreshStyle();
			}
			if (ImGui::MenuItem("Titan", NULL, styleId == 3)) {
				styleId = 3;
				refreshStyle();
			}
			ImGui::EndMenu();
		}
		// Help
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("Manual PDF", "F1", false))
				menuManual();
			if (ImGui::MenuItem("Webpage", "", false))
				menuWebsite();
			// if (ImGui::MenuItem("imgui Demo", NULL, showTestWindow)) showTestWindow = !showTestWindow;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}


void renderPreview(PlaySource *play_source) {
	ImGui::Checkbox("Play", &playEnabled);
	ImGui::SameLine();
	ImGui::PushItemWidth(300.0);
	
	if (ImGui::RadioButton("Wave", *play_source == PLAY_WAVE)) *play_source = PLAY_WAVE;
	ImGui::PushItemWidth(300.0);
	ImGui::SameLine();
	if (ImGui::RadioButton("Carrier", *play_source == PLAY_CARRIER)) *play_source = PLAY_CARRIER;
	ImGui::SameLine();
	if (ImGui::RadioButton("Modulator", *play_source == PLAY_MODULATOR)) *play_source = PLAY_MODULATOR;
	ImGui::SameLine();
	
	ImGui::SliderFloat("##playVolume", &playVolume, -60.0f, 0.0f, "Volume: %.2f dB");
	ImGui::PushItemWidth(-1.0);
	ImGui::SameLine();
	ImGui::SliderFloat("##playFrequency", &playFrequency, 1.0f, 10000.0f, "Frequency: %.2f Hz", 0.0f);

	ImGui::Checkbox("Morph Interpolate", &morphInterpolate);
	if (playModeXY) {
		ImGui::SameLine();
		ImGui::PushItemWidth(-1.0);
		float width = ImGui::CalcItemWidth() / 2.0 - ImGui::GetStyle().FramePadding.y;
		ImGui::PushItemWidth(width);
		ImGui::SliderFloat("##Morph X", &morphX, 0.0, BANK_GRID_WIDTH - 1, "Morph X: %.3f");
		ImGui::SameLine();
		ImGui::SliderFloat("##Morph Y", &morphY, 0.0, BANK_GRID_HEIGHT - 1, "Morph Y: %.3f");
	}
	else {
		ImGui::SameLine();
		ImGui::PushItemWidth(-1.0);
		float width = ImGui::CalcItemWidth() / 2.0 - ImGui::GetStyle().FramePadding.y;
		ImGui::PushItemWidth(width);
		ImGui::SliderFloat("##Morph Z", &morphZ, 0.0, BANK_LEN - 1, "Morph Z: %.3f");
		ImGui::SameLine();
		ImGui::SliderFloat("##Morph Z Speed", &morphZSpeed, 0.f, 10.f, "Morph Z Speed: %.3f Hz", 3.f);
	}

	refreshMorphSnap();
}


void renderToolSelector(Tool *tool) {
	if (ImGui::RadioButton("Pencil", *tool == PENCIL_TOOL)) *tool = PENCIL_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Brush", *tool == BRUSH_TOOL)) *tool = BRUSH_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Grab", *tool == GRAB_TOOL)) *tool = GRAB_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Line", *tool == LINE_TOOL)) *tool = LINE_TOOL;
	ImGui::SameLine();
	if (ImGui::RadioButton("Eraser", *tool == ERASER_TOOL)) *tool = ERASER_TOOL;
}


void effectSlider(EffectID effect) {
	char id[64];
	snprintf(id, sizeof(id), "##%s", effectNames[effect]);
	char text[64];
	snprintf(text, sizeof(text), "%s: %%.3f", effectNames[effect]);
	if (ImGui::SliderFloat(id, &currentBank.waves[selectedId].effects[effect], 0.0f, 1.0f, text)) {
		currentBank.waves[selectedId].updatePost();
		historyPush();
	}
}


void editorPage() {
	ImGui::BeginChild("Sidebar", ImVec2(200, 0), true);
	{
		float dummyZ = 0.0;
		ImGui::PushItemWidth(-1);
		renderBankGrid("SidebarGrid", BANK_LEN * 35.0, 1, &dummyZ, &morphZ);
		refreshMorphSnap();
	}
	ImGui::EndChild();

	ImGui::SameLine();
	ImGui::BeginChild("Editor", ImVec2(0, 0), true);
	{
		Wave *wave = &currentBank.waves[selectedId];
		float *effects = wave->effects;

		ImGui::PushItemWidth(-1);

		static enum Tool tool = PENCIL_TOOL;
		renderToolSelector(&tool);

		ImGui::SameLine();
		if (ImGui::Button("Clear")) {
			currentBank.waves[selectedId].clear();
			historyPush();
		}


		for (const CatalogCategory &catalogCategory : catalogCategories) {
			ImGui::SameLine();
			if (ImGui::Button(catalogCategory.name.c_str())) ImGui::OpenPopup(catalogCategory.name.c_str());
			if (ImGui::BeginPopup(catalogCategory.name.c_str())) {
				for (const CatalogFile &catalogFile : catalogCategory.files) {
					if (ImGui::Selectable(catalogFile.name.c_str())) {
						memcpy(currentBank.waves[selectedId].samples, catalogFile.samples, sizeof(float) * WAVE_LEN);
						currentBank.waves[selectedId].commitSamples();
						historyPush();
					}
				}
				ImGui::EndPopup();
			}
		}

		// ImGui::SameLine();
		// if (ImGui::RadioButton("Smooth", tool == SMOOTH_TOOL)) tool = SMOOTH_TOOL;

		ImGui::Text("Waveform");
		const int oversample = 4;
		float waveOversample[WAVE_LEN * oversample];
		cyclicOversample(wave->postSamples, waveOversample, WAVE_LEN, oversample);
		if (renderWave("WaveEditor", 200.0, wave->samples, WAVE_LEN, waveOversample, WAVE_LEN * oversample, tool)) {
			currentBank.waves[selectedId].commitSamples();
			historyPush();
		}

		ImGui::Text("Harmonics");
		if (renderHistogram("HarmonicEditor", 200.0, wave->harmonics, WAVE_LEN / 2, wave->postHarmonics, WAVE_LEN / 2, tool)) {
			currentBank.waves[selectedId].commitHarmonics();
			historyPush();
		}

		ImGui::Text("Effects");
		for (int i = 0; i < EFFECTS_LEN; i++) {
			effectSlider((EffectID) i);
		}

		if (ImGui::Checkbox("Cycle", &currentBank.waves[selectedId].cycle)) {
			currentBank.waves[selectedId].updatePost();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Checkbox("Normalize", &currentBank.waves[selectedId].normalize)) {
			currentBank.waves[selectedId].updatePost();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Randomize")) {
			currentBank.waves[selectedId].randomizeEffects();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			currentBank.waves[selectedId].clearEffects();
			historyPush();
		}
		ImGui::SameLine();
		if (ImGui::Button("Bake")) {
			currentBank.waves[selectedId].bakeEffects();
			historyPush();
		}

		ImGui::PopItemWidth();
	}
	ImGui::EndChild();
}


void effectHistogram(EffectID effect, Tool tool) {
	float value[BANK_LEN];
	float average = 0.0;
	for (int i = 0; i < BANK_LEN; i++) {
		value[i] = currentBank.waves[i].effects[effect];
		average += value[i];
	}
	average /= BANK_LEN;
	float oldAverage = average;

	ImGui::Text("%s", effectNames[effect]);

	char id[64];
	snprintf(id, sizeof(id), "##%sAverage", effectNames[effect]);
	char text[64];
	snprintf(text, sizeof(text), "Average %s: %%.3f", effectNames[effect]);
	if (ImGui::SliderFloat(id, &average, 0.0f, 1.0f, text)) {
		// Change the average effect level to the new average
		float deltaAverage = average - oldAverage;
		for (int i = 0; i < BANK_LEN; i++) {
			if (0.0 < average && average < 1.0) {
				currentBank.waves[i].effects[effect] = clampf(currentBank.waves[i].effects[effect] + deltaAverage, 0.0, 1.0);
			}
			else {
				currentBank.waves[i].effects[effect] = average;
			}
			currentBank.waves[i].updatePost();
			historyPush();
		}
	}

	if (renderHistogram(effectNames[effect], 120, value, BANK_LEN, NULL, 0, tool)) {
		for (int i = 0; i < BANK_LEN; i++) {
			if (currentBank.waves[i].effects[effect] != value[i]) {
				// TODO This always selects the highest index. Select the index the mouse is hovering (requires renderHistogram() to return an int)
				selectWave(i);
				currentBank.waves[i].effects[effect] = value[i];
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
	}
}


void effectPage() {
	ImGui::BeginChild("Effect Editor", ImVec2(0, 0), true); {
		static Tool tool = PENCIL_TOOL;
		renderToolSelector(&tool);

		ImGui::PushItemWidth(-1);
		for (int i = 0; i < EFFECTS_LEN; i++) {
			effectHistogram((EffectID) i, tool);
		}
		ImGui::PopItemWidth();

		if (ImGui::Button("Cycle All")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].cycle = true;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Cycle None")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].cycle = false;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Normalize All")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].normalize = true;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Normalize None")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].normalize = false;
				currentBank.waves[i].updatePost();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Randomize")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].randomizeEffects();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].clearEffects();
				historyPush();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Bake")) {
			for (int i = 0; i < BANK_LEN; i++) {
				currentBank.waves[i].bakeEffects();
				historyPush();
			}
		}
	}
	ImGui::EndChild();
}


void gridPage() {
	playModeXY = true;
	ImGui::BeginChild("Grid Page", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		renderBankGrid("WaveGrid", -1.f, BANK_GRID_WIDTH, &morphX, &morphY);
		refreshMorphSnap();
	}
	ImGui::EndChild();
}


void waterfallPage() {
	ImGui::BeginChild("3D View", ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		static float amplitude = 0.25;
		ImGui::SliderFloat("##amplitude", &amplitude, 0.01, 1.0, "Scale: %.3f", 0.0);
		static float angle = 1.0;
		ImGui::SliderFloat("##angle", &angle, 0.0, 1.0, "Angle: %.3f");

		renderWaterfall("##waterfall", -1.0, amplitude, angle, &morphZ);
	}
	ImGui::EndChild();
}


void baseWavePage(BaseWave *wave, const char* title, bool update_waves) {
	ImGui::BeginChild("Sidebar", ImVec2(200, 0), true);
	{
		float dummyZ = 0.0;
		ImGui::PushItemWidth(-1);
		renderBankGrid("SidebarGrid", BANK_LEN * 35.0, 1, &dummyZ, &morphZ);
		refreshMorphSnap();
	}
	ImGui::EndChild();

	ImGui::SameLine();
	
	ImGui::BeginChild(title, ImVec2(0, 0), true);
	{
		ImGui::PushItemWidth(-1.0);
		static Tool tool = PENCIL_TOOL;
		renderToolSelector(&tool);
		
	
		const int oversample = 4;
		
		ImGui::Text("Final Waveform");
		//float samplesOversample[WAVE_LEN * oversample];
		//cyclicOversample(wave->samples, samplesOversample, WAVE_LEN, oversample);
		if (renderWave("FinalWaveEditor", 200.0, wave->samples, WAVE_LEN, wave->samples, WAVE_LEN, tool)) {
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		ImGui::Text("Harmonics");
		float x[WAVE_LEN / 2] = {};
		renderHistogram("Harmonics View", 200.0, wave->harmonics, WAVE_LEN / 2, nullptr, 0, NO_TOOL);
		
        ImGui::Columns(2);
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f - 20);
		ImGui::Text("Base Waveform");
        
		float shapeOversample[WAVE_LEN * oversample];
		cyclicOversample(wave->shape, shapeOversample, WAVE_LEN, oversample);
		if (renderWave("WaveEditor", 200.0, wave->shape, WAVE_LEN, shapeOversample, WAVE_LEN * oversample, tool)) {
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		ImGui::NextColumn();
        ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.5f - 20);
		ImGui::Text("Phasor");
        
//		ImGui::SameLine();
		float phasorBuf[WAVE_LEN];
		memcpy(phasorBuf, wave->phasor, sizeof(float) * WAVE_LEN);

		//cyclicOversample(wave->phasor, phasorOversample, WAVE_LEN, oversample);
		if (renderPhasor("PhasorEditor", 200.0, wave->phasor, WAVE_LEN, phasorBuf, WAVE_LEN, tool, wave->bottom_x, wave->bottom_y, wave->bottom_magnitude, wave->top_x, wave->top_y, wave->top_magnitude)) {
			wave->generateSamples(update_waves);
			historyPush();
		}
		ImGui::Columns();
		
        ImGui::PushItemWidth(ImGui::GetWindowWidth());
		
		if (ImGui::Checkbox("Lock Shapes", &(wave->lock_shapes))) {
			wave->updateShape();
			wave->generateSamples(update_waves);
			historyPush();
		};
		
		if (ImGui::SliderFloat("##lower_shape", &(wave->lower_shape), 0.0, 1.0, "Lower shape: %.3f")) {
			if (wave->lock_shapes)
				wave->upper_shape = wave->lower_shape;
			wave->updateShape();
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		if (wave->lock_shapes) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		
		if (ImGui::SliderFloat("##upper_shape", &(wave->upper_shape), 0.0, 1.0, "Upper shape: %.3f")) {
			wave->updateShape();
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		if (wave->lock_shapes) {
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
		
		if (ImGui::SliderFloat("##pulse_width", &(wave->pulse_width), 0.0, 1.0, "Pulse width: %.3f")) {
			wave->updateShape();
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		if (ImGui::SliderFloat("##shape_brightness", &(wave->brightness), 0.0, 1.0, "Shape brightness: %.3f")) {
			wave->updateShape();
			wave->generateSamples(update_waves);
			historyPush();
		}

		if (ImGui::SliderFloat("##bottom_angle", &(wave->bottom_angle), 0.0, 1.0, "Bottom angle: %.3f")) {
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		if (ImGui::SliderFloat("##bottom_magnitude", &(wave->bottom_magnitude), 0.0, 1.0, "Bottom magnitude: %.3f")) {
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		if (ImGui::SliderFloat("##top_angle", &(wave->top_angle), 0.0, 1.0, "Top angle: %.3f")) {
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		if (ImGui::SliderFloat("##top_magnitude", &(wave->top_magnitude), 0.0, 1.0, "Top magnitude: %.3f")) {
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		if (ImGui::SliderFloat("##bezier_weight", &(wave->bezier_weight), 0.0, 1.0, "Bezier weight: %.3f")) {
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}
		
		if (ImGui::SliderFloat("##bezier_ratio", &(wave->bezier_ratio), 0.0, 1.0, "Bezier ratio: %.3f")) {
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}
				
		if (ImGui::RadioButton("Resonant", wave->multi_algo == MUL_RESONANT)) {
			wave->multi_algo = MUL_RESONANT;            
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}

		ImGui::SameLine();
		if (ImGui::RadioButton("Divisor-modulo", wave->multi_algo == MUL_DIV_MOD)) {
			wave->multi_algo = MUL_DIV_MOD;
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}

		ImGui::SameLine();
		if (ImGui::RadioButton("Harmonic", wave->multi_algo == MUL_HARMONIC)) {
			wave->multi_algo = MUL_HARMONIC;
			wave->updatePhasor();
			wave->generateSamples(update_waves);
			historyPush();
		}
		//static float resonance = 0.0;
		//			if (ImGui::SliderFloat(id, &currentBank.waves[selectedId].effects[effect], 0.0f, 1.0f, text)) {
		if (ImGui::SliderFloat("##resonance", &(wave->resonance), 0.0, 1.0, "Resonance: %.3f")) {
			//wave->resonance = clampf(wave.resonance, 0.0, 1.0);
			wave->generateSamples(update_waves);
			historyPush();
		}
	}
	ImGui::EndChild();
}


void carrierWavePage() {
	baseWavePage(&(currentBank.carrier_wave), "Carrier Wave", true);
}


void modulatorWavePage() {
	baseWavePage(&(currentBank.modulator_wave), "Modulator Wave", false);
}


void renderMain() {
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2((int)ImGui::GetIO().DisplaySize.x, (int)ImGui::GetIO().DisplaySize.y));

	ImGui::Begin("WaveEdit", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_MenuBar);
	{
		// Menu bar
		renderMenu();

		renderPreview(&playSource);
		// Tab bar
		{
			static const char *tabLabels[NUM_PAGES] = {
				"Carrier Wave Editor",
				"Modulator Wave Editor",
				"Waveform Editor",
				"Effect Editor",
				"Grid XY View",
				"Waterfall View",
				"Import",
				"WaveEdit Online"
			};
			static int hoveredTab = 0;
			ImGui::TabLabels(NUM_PAGES, tabLabels, (int*)&currentPage, NULL, false, &hoveredTab);
		}

		// Page
		// Reset some audio variables. These might be changed within the pages.
		playModeXY = false;
		playingBank = &currentBank;
		switch (currentPage) {
		case CARRIER_WAVE_PAGE: carrierWavePage(); break;
		case MODULATOR_WAVE_PAGE: modulatorWavePage(); break;
		case EDITOR_PAGE: editorPage(); break;
		case EFFECT_PAGE: effectPage(); break;
		case GRID_PAGE: gridPage(); break;
		case WATERFALL_PAGE: waterfallPage(); break;
		case IMPORT_PAGE: importPage(); break;
		case DB_PAGE: dbPage(); break;
		default: break;
		}
	}
	ImGui::End();

	if (showTestWindow) {
		ImGui::ShowTestWindow();
	}
}


static void refreshStyle() {
	const ImVec4 transparent = ImVec4(0.0, 0.0, 0.0, 0.0);

	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.f;
	style.WindowRounding = 2.f;
	style.GrabRounding = 2.f;
	style.ChildRounding = 2.f;
	style.ScrollbarRounding = 2.f;
	style.FrameRounding = 2.f;
	style.FramePadding = ImVec2(6.0f, 4.0f);

	if (styleId == 0) {
		style.Colors[ImGuiCol_Text]                  = ImVec4(0.73f, 0.73f, 0.73f, 1.00f);
		style.Colors[ImGuiCol_TextDisabled]          = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
		style.Colors[ImGuiCol_WindowBg]              = ImVec4(0.26f, 0.26f, 0.26f, 0.95f);
		style.Colors[ImGuiCol_ChildWindowBg]         = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_PopupBg]               = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_Border]                = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_BorderShadow]          = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
		style.Colors[ImGuiCol_FrameBg]               = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_FrameBgActive]         = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);
		style.Colors[ImGuiCol_TitleBg]               = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_TitleBgActive]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_MenuBarBg]             = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_CheckMark]             = ImVec4(0.78f, 0.78f, 0.78f, 1.00f);
		style.Colors[ImGuiCol_SliderGrab]            = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
		style.Colors[ImGuiCol_SliderGrabActive]      = ImVec4(0.74f, 0.74f, 0.74f, 1.00f);
		style.Colors[ImGuiCol_Button]                = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ButtonHovered]         = ImVec4(0.43f, 0.43f, 0.43f, 1.00f);
		style.Colors[ImGuiCol_ButtonActive]          = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
		style.Colors[ImGuiCol_Header]                = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_HeaderHovered]         = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive]          = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		// style.Colors[ImGuiCol_Column]                = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		// style.Colors[ImGuiCol_ColumnHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		// style.Colors[ImGuiCol_ColumnActive]          = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		style.Colors[ImGuiCol_ResizeGrip]            = ImVec4(0.36f, 0.36f, 0.36f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		style.Colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		style.Colors[ImGuiCol_PlotLines]             = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		style.Colors[ImGuiCol_PlotLinesHovered]      = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		style.Colors[ImGuiCol_PlotHistogram]         = ImVec4(1.0, 0.8, 0.2, 1.0);
		style.Colors[ImGuiCol_PlotHistogramHovered]  = ImVec4(0.7, 0.5, 0.1, 0.5);
		style.Colors[ImGuiCol_TextSelectedBg]        = ImVec4(0.32f, 0.52f, 0.65f, 1.00f);
		style.Colors[ImGuiCol_ModalWindowDarkening]  = ImVec4(0.20f, 0.20f, 0.20f, 0.50f);
		logoTexture = logoTextureLight;
	}
	else if (styleId == 1) {
		// base16-atelier-dune
		ImVec4 base[16] = {
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x20, 0x20, 0x1d, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x29, 0x28, 0x24, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x6e, 0x6b, 0x5e, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x7d, 0x7a, 0x68, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x99, 0x95, 0x80, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xa6, 0xa2, 0x8c, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xe8, 0xe4, 0xcf, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xfe, 0xfb, 0xec, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd7, 0x37, 0x37, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb6, 0x56, 0x11, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xae, 0x95, 0x13, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x60, 0xac, 0x39, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x1f, 0xad, 0x83, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x66, 0x84, 0xe1, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb8, 0x54, 0xd4, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd4, 0x35, 0x52, 0xff)),
		};

		style.Colors[ImGuiCol_Text]                 = base[0x6];
		style.Colors[ImGuiCol_TextDisabled]         = base[0x4];
		style.Colors[ImGuiCol_WindowBg]             = base[0x2];
		style.Colors[ImGuiCol_ChildWindowBg]        = base[0x2];
		style.Colors[ImGuiCol_PopupBg]              = base[0x2];
		style.Colors[ImGuiCol_Border]               = transparent;
		style.Colors[ImGuiCol_BorderShadow]         = transparent;
		style.Colors[ImGuiCol_FrameBg]              = base[0x1];
		style.Colors[ImGuiCol_FrameBgHovered]       = base[0x1];
		style.Colors[ImGuiCol_FrameBgActive]        = base[0x1];
		style.Colors[ImGuiCol_TitleBg]              = base[0x3];
		style.Colors[ImGuiCol_TitleBgCollapsed]     = base[0x3];
		style.Colors[ImGuiCol_TitleBgActive]        = base[0x3];
		style.Colors[ImGuiCol_MenuBarBg]            = base[0x2];
		style.Colors[ImGuiCol_ScrollbarBg]          = base[0x3];
		style.Colors[ImGuiCol_ScrollbarGrab]        = base[0x4];
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[0x4];
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = base[0x4];
		style.Colors[ImGuiCol_CheckMark]            = base[0x4];
		style.Colors[ImGuiCol_SliderGrab]           = base[0x4];
		style.Colors[ImGuiCol_SliderGrabActive]     = base[0x4];
		style.Colors[ImGuiCol_Button]               = base[0x3];
		style.Colors[ImGuiCol_ButtonHovered]        = base[0x4];
		style.Colors[ImGuiCol_ButtonActive]         = base[0x4];
		style.Colors[ImGuiCol_Header]               = base[0x3];
		style.Colors[ImGuiCol_HeaderHovered]        = base[0x3];
		style.Colors[ImGuiCol_HeaderActive]         = base[0x3];
		// style.Colors[ImGuiCol_Column]               = base[0x2];
		// style.Colors[ImGuiCol_ColumnHovered]        = base[0x2];
		// style.Colors[ImGuiCol_ColumnActive]         = base[0x2];
		style.Colors[ImGuiCol_ResizeGrip]           = base[0x2];
		style.Colors[ImGuiCol_ResizeGripHovered]    = base[0x2];
		style.Colors[ImGuiCol_ResizeGripActive]     = base[0x2];
		style.Colors[ImGuiCol_PlotLines]            = base[0x4];
		style.Colors[ImGuiCol_PlotLinesHovered]     = base[0x4];
		style.Colors[ImGuiCol_PlotHistogram]        = darken(base[0x8], 0.2);
		style.Colors[ImGuiCol_PlotHistogramHovered] = alpha(base[0x7], 0.2);
		style.Colors[ImGuiCol_TextSelectedBg]       = base[0x3];
		style.Colors[ImGuiCol_ModalWindowDarkening] = alpha(base[0x1], 0.5);
		logoTexture = logoTextureLight;
	}
	else if (styleId == 2) {
		// base16-atelier-dune
		ImVec4 base[16] = {
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x20, 0x20, 0x1d, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x29, 0x28, 0x24, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x6e, 0x6b, 0x5e, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x7d, 0x7a, 0x68, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x99, 0x95, 0x80, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xa6, 0xa2, 0x8c, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xe8, 0xe4, 0xcf, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xfe, 0xfb, 0xec, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd7, 0x37, 0x37, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb6, 0x56, 0x11, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xae, 0x95, 0x13, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x60, 0xac, 0x39, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x1f, 0xad, 0x83, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x66, 0x84, 0xe1, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb8, 0x54, 0xd4, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xd4, 0x35, 0x52, 0xff)),
		};

		style.Colors[ImGuiCol_Text]                 = base[0x1];
		style.Colors[ImGuiCol_TextDisabled]         = base[0x2];
		style.Colors[ImGuiCol_WindowBg]             = base[0x7];
		style.Colors[ImGuiCol_ChildWindowBg]        = base[0x7];
		style.Colors[ImGuiCol_PopupBg]              = base[0x7];
		style.Colors[ImGuiCol_Border]               = transparent;
		style.Colors[ImGuiCol_BorderShadow]         = transparent;
		style.Colors[ImGuiCol_FrameBg]              = base[0x6];
		style.Colors[ImGuiCol_FrameBgHovered]       = base[0x6];
		style.Colors[ImGuiCol_FrameBgActive]        = base[0x6];
		style.Colors[ImGuiCol_TitleBg]              = base[0x4];
		style.Colors[ImGuiCol_TitleBgCollapsed]     = base[0x4];
		style.Colors[ImGuiCol_TitleBgActive]        = base[0x4];
		style.Colors[ImGuiCol_MenuBarBg]            = base[0x7];
		style.Colors[ImGuiCol_ScrollbarBg]          = base[0x6];
		style.Colors[ImGuiCol_ScrollbarGrab]        = base[0x5];
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[0x5];
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = base[0x5];
		style.Colors[ImGuiCol_CheckMark]            = base[0x5];
		style.Colors[ImGuiCol_SliderGrab]           = base[0x5];
		style.Colors[ImGuiCol_SliderGrabActive]     = base[0x5];
		style.Colors[ImGuiCol_Button]               = base[0x5];
		style.Colors[ImGuiCol_ButtonHovered]        = base[0x6];
		style.Colors[ImGuiCol_ButtonActive]         = base[0x6];
		style.Colors[ImGuiCol_Header]               = base[0x6];
		style.Colors[ImGuiCol_HeaderHovered]        = base[0x5];
		style.Colors[ImGuiCol_HeaderActive]         = base[0x5];
		// style.Colors[ImGuiCol_Column]               = base[0x2];
		// style.Colors[ImGuiCol_ColumnHovered]        = base[0x2];
		// style.Colors[ImGuiCol_ColumnActive]         = base[0x2];
		style.Colors[ImGuiCol_ResizeGrip]           = base[0x2];
		style.Colors[ImGuiCol_ResizeGripHovered]    = base[0x2];
		style.Colors[ImGuiCol_ResizeGripActive]     = base[0x2];
		style.Colors[ImGuiCol_PlotLines]            = base[0x4];
		style.Colors[ImGuiCol_PlotLinesHovered]     = base[0x4];
		style.Colors[ImGuiCol_PlotHistogram]        = base[0xc];
		style.Colors[ImGuiCol_PlotHistogramHovered] = alpha(base[0x5], 0.8);
		style.Colors[ImGuiCol_TextSelectedBg]       = base[0x3];
		style.Colors[ImGuiCol_ModalWindowDarkening] = alpha(base[0x2], 0.5);
		logoTexture = logoTextureDark;
	}
	else if (styleId == 3) {
		// base16-ashes
		ImVec4 base[16] = {
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x0c, 0x0d, 0x0e, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x2e, 0x2f, 0x30, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x51, 0x52, 0x53, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x73, 0x74, 0x75, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x95, 0x96, 0x97, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb7, 0xb8, 0xb9, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xda, 0xdb, 0xdc, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xfc, 0xfd, 0xfe, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xe3, 0x1a, 0x1c, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xe6, 0x55, 0x0d, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xdc, 0xa0, 0x60, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x31, 0xa3, 0x54, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x80, 0xb1, 0xd3, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x31, 0x82, 0xbd, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0x75, 0x6b, 0xb1, 0xff)),
			ImGui::ColorConvertU32ToFloat4(IM_COL32(0xb1, 0x59, 0x28, 0xff)),
		};

		style.Colors[ImGuiCol_Text]                 = base[0x1];
		style.Colors[ImGuiCol_TextDisabled]         = base[0x2];
		style.Colors[ImGuiCol_WindowBg]             = base[0x7];
		style.Colors[ImGuiCol_ChildWindowBg]        = base[0x7];
		style.Colors[ImGuiCol_PopupBg]              = base[0x7];
		style.Colors[ImGuiCol_Border]               = transparent;
		style.Colors[ImGuiCol_BorderShadow]         = transparent;
		style.Colors[ImGuiCol_FrameBg]              = base[0x6];
		style.Colors[ImGuiCol_FrameBgHovered]       = base[0x6];
		style.Colors[ImGuiCol_FrameBgActive]        = base[0x6];
		style.Colors[ImGuiCol_TitleBg]              = base[0x4];
		style.Colors[ImGuiCol_TitleBgCollapsed]     = base[0x4];
		style.Colors[ImGuiCol_TitleBgActive]        = base[0x4];
		style.Colors[ImGuiCol_MenuBarBg]            = base[0x7];
		style.Colors[ImGuiCol_ScrollbarBg]          = base[0x6];
		style.Colors[ImGuiCol_ScrollbarGrab]        = base[0x5];
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = base[0x5];
		style.Colors[ImGuiCol_ScrollbarGrabActive]  = base[0x5];
		style.Colors[ImGuiCol_CheckMark]            = base[0x5];
		style.Colors[ImGuiCol_SliderGrab]           = base[0x5];
		style.Colors[ImGuiCol_SliderGrabActive]     = base[0x5];
		style.Colors[ImGuiCol_Button]               = base[0x5];
		style.Colors[ImGuiCol_ButtonHovered]        = base[0x6];
		style.Colors[ImGuiCol_ButtonActive]         = base[0x6];
		style.Colors[ImGuiCol_Header]               = base[0x6];
		style.Colors[ImGuiCol_HeaderHovered]        = base[0x5];
		style.Colors[ImGuiCol_HeaderActive]         = base[0x5];
		// style.Colors[ImGuiCol_Column]               = base[0x2];
		// style.Colors[ImGuiCol_ColumnHovered]        = base[0x2];
		// style.Colors[ImGuiCol_ColumnActive]         = base[0x2];
		style.Colors[ImGuiCol_ResizeGrip]           = base[0x2];
		style.Colors[ImGuiCol_ResizeGripHovered]    = base[0x2];
		style.Colors[ImGuiCol_ResizeGripActive]     = base[0x2];
		style.Colors[ImGuiCol_PlotLines]            = base[0x4];
		style.Colors[ImGuiCol_PlotLinesHovered]     = base[0x4];
		style.Colors[ImGuiCol_PlotHistogram]        = base[0xd];
		style.Colors[ImGuiCol_PlotHistogramHovered] = alpha(base[0xc], 0.8);
		style.Colors[ImGuiCol_TextSelectedBg]       = base[0x3];
		style.Colors[ImGuiCol_ModalWindowDarkening] = alpha(base[0x2], 0.5);
		logoTexture = logoTextureDark;
	}
}


void uiInit() {
	ImGui::GetIO().IniFilename = NULL;
	styleId = 3;

	// Load fonts
	ImGui::GetIO().Fonts->AddFontFromFileTTF("fonts/Lekton-Regular.ttf", 15.0);
	logoTextureLight = loadImage("logo-light.png");
	logoTextureDark = loadImage("logo-dark.png");

	// Load UI settings
	// If this gets any more complicated, it should be JSON.
	{
		FILE *f = fopen("ui.dat", "rb");
		if (f) {
			fread(&styleId, sizeof(styleId), 1, f);
			fclose(f);
		}
	}

	refreshStyle();
}


void uiDestroy() {
	// Save UI settings
	{
		FILE *f = fopen("ui.dat", "wb");
		if (f) {
			fwrite(&styleId, sizeof(styleId), 1, f);
			fclose(f);
		}
	}
}


void uiRender() {
	renderMain();
}
