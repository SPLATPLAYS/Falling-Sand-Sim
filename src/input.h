#ifndef INPUT_H
#define INPUT_H

#include "config.h"

// Current selected particle
extern Particle selectedParticle;

// Current brush size (1-BRUSH_SIZE_MAX), persisted via MCS
extern int brushSize;

// Handle input, returns true if should exit
bool handleInput();

#endif // INPUT_H
