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

#endif // INPUT_H
