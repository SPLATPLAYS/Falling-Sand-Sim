#ifndef GRID_H
#define GRID_H

#include "config.h"

// Global grid - aligned for better cache performance
extern Particle grid[GRID_HEIGHT][GRID_WIDTH];
extern bool updated[GRID_HEIGHT][GRID_WIDTH]; // Track which cells were updated this frame
extern uint8_t temperature[GRID_HEIGHT][GRID_WIDTH]; // Temperature value for each cell (0-255)

// Initialize the grid
void initGrid();

// Check if coordinates are valid
bool isValid(int x, int y);

// Check if a cell is empty (air)
bool isEmpty(int x, int y);

// Check if a particle can move to a position
bool canMoveTo(int x, int y, Particle type);

// Swap two particles
void swap(int x1, int y1, int x2, int y2);

#endif // GRID_H
