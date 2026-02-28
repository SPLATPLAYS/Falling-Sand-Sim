#ifndef GRID_H
#define GRID_H

#include "config.h"

// Words per row for the updated bitset
constexpr int UPDATED_WORDS = (GRID_WIDTH + 31) / 32;

// Global grid - aligned for better cache performance
extern Particle grid[GRID_HEIGHT][GRID_WIDTH];
extern uint32_t updated[GRID_HEIGHT][UPDATED_WORDS]; // Bitset: 1 bit per cell (576 bytes vs 18 KB)
extern uint8_t temperature[TEMP_GRID_H][TEMP_GRID_W]; // Coarse temperature grid (1,152 bytes vs 18 KB)

// Coarse temperature accessors (fine-cell coordinates)
inline uint8_t tempGet(int x, int y) {
  return temperature[y / TEMP_SCALE][x / TEMP_SCALE];
}
inline void tempSet(int x, int y, uint8_t val) {
  temperature[y / TEMP_SCALE][x / TEMP_SCALE] = val;
}

// Bitset helpers
inline bool updatedGet(int x, int y) {
  return (updated[y][x >> 5] >> (x & 31)) & 1u;
}
inline void updatedSet(int x, int y) {
  updated[y][x >> 5] |= (1u << (x & 31));
}

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
