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
// Returns  1 : user pressed Play (start game)
//         -1 : user wants to exit the app
//          0 : no relevant event yet
int handleStartMenuInput();

#endif // INPUT_H
