#ifndef INPUT_H
#define INPUT_H

#include "config.h"

// Current selected particle
extern Particle selectedParticle;

// Current brush size (1-BRUSH_SIZE_MAX), persisted via MCS
extern int brushSize;

// Toggle temperature heat-map overlay (0 key)
extern bool tempViewEnabled;

// Handle input, returns true if should exit
bool handleInput();

// Handle start-menu input.
// Returns  1 : PLAY pressed  (start game)
//          2 : SETTINGS pressed
//         -1 : EXIT pressed / app-level exit requested
//          0 : no relevant event yet
int handleStartMenuInput();

// Handle top-level settings menu input.
// 'selectedItem' cycles over {0=CPU SPEED, 1=SIM SPEED}.
// Returns  1 : EXE pressed (enter selected sub-menu)
//         -1 : CLEAR/ESC (go back to start menu)
//          0 : nothing completed yet
int handleSettingsMenuInput(int& selectedItem);

// Handle overclock sub-menu input.
// Returns  1 : confirmed   -1 : cancelled   0 : navigating
int handleOCInput(int& selectedLevel);

// Handle simulation speed sub-menu input.
// Returns  1 : confirmed   -1 : cancelled   0 : navigating
int handleSimSpeedInput(int& selectedMode);

#endif // INPUT_H
