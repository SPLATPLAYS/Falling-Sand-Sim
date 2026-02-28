#ifndef RENDERER_H
#define RENDERER_H

#include <cstdint>

// Actual LCD dimensions (set at runtime)
extern int lcdWidth;
extern int lcdHeight;

// Initialize renderer with LCD dimensions
void initRenderer(int width, int height);

// Update FPS counter
void updateFPS();

// Draw the grid to screen
void drawGrid(uint16_t* vram);

#endif // RENDERER_H
