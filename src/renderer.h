#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>
#include "config.h"

// Actual LCD dimensions (set at runtime)
extern int lcdWidth;
extern int lcdHeight;

// Initialize renderer with LCD dimensions
void initRenderer(int width, int height);

// Update FPS counter
void updateFPS();

// Draw the grid to screen
ILRAM_FUNC void drawGrid(uint16_t* vram);

// Start menu button bounds (updated by drawStartMenu on every call)
extern int startMenuPlayBtnX,     startMenuPlayBtnY,     startMenuPlayBtnW,     startMenuPlayBtnH;
extern int startMenuSettingsBtnX, startMenuSettingsBtnY, startMenuSettingsBtnW, startMenuSettingsBtnH;
extern int startMenuExitBtnX,     startMenuExitBtnY,     startMenuExitBtnW,     startMenuExitBtnH;

// Draw the start menu (title + PLAY / SETTINGS / EXIT buttons)
void drawStartMenu(uint16_t* vram);

// Draw the top-level settings menu (CPU SPEED / SIM SPEED rows).
// 'selectedItem' is the currently highlighted row (0 or 1).
void drawSettingsMenu(uint16_t* vram, int selectedItem);

// Draw the overclock sub-menu.
void drawOCScreen(uint16_t* vram, int selectedLevel);

// Draw the simulation speed sub-menu.
void drawSimSpeedScreen(uint16_t* vram, int selectedMode);

#endif // RENDERER_H
