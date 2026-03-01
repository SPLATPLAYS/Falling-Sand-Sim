#ifndef SETTINGS_H
#define SETTINGS_H

#include "overclock.h"
#include "config.h"

// Persisted overclock level (0 = default, 1-4 = progressively overclocked).
extern int overclockLevel;

// Persisted simulation speed mode (0 = full rate, 1-4 = progressively more skipping).
extern int simSpeedMode;

// Initialize settings: create MCS folder and load all persisted values.
void initSettings();

// Persist current brush size to MCS
void saveBrushSize();

// Persist current overclock level to MCS
void saveOverclockLevel();

// Persist current simulation speed mode to MCS
void saveSimSpeedMode();

#endif // SETTINGS_H
