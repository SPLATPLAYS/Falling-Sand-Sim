#include "grid.h"
#include <cstring>

// Grid split across on-chip X/Y RAM — section names match the SDK linker script (same as CPBoy)
Particle gridX[GRID_ROWS_X][GRID_WIDTH]       __attribute__((section(".oc_mem.x.data")));
Particle gridY[GRID_ROWS_Y][GRID_WIDTH]       __attribute__((section(".oc_mem.y.data")));
Particle gridRest[GRID_ROWS_REST][GRID_WIDTH]; // remaining rows in regular RAM
// Row-pointer table (built in initGrid)
Particle *grid[GRID_HEIGHT];
alignas(32) uint32_t updated[GRID_HEIGHT][UPDATED_WORDS]; // Bitset: 1 bit per cell
alignas(32) uint8_t temperature[TEMP_GRID_H][TEMP_GRID_W]; // Coarse temperature grid (1,152 bytes)

// Initialize the grid
void initGrid() {
  // Build row-pointer table
  for (int y = 0; y < GRID_ROWS_X; y++)
    grid[y] = gridX[y];
  for (int y = 0; y < GRID_ROWS_Y; y++)
    grid[GRID_ROWS_X + y] = gridY[y];
  for (int y = 0; y < GRID_ROWS_REST; y++)
    grid[GRID_ROWS_X + GRID_ROWS_Y + y] = gridRest[y];

  memset(updated, 0, sizeof(updated));
  memset(temperature, TEMP_AMBIENT, sizeof(temperature));
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      grid[y][x] = Particle::AIR;
    }
  }
  
  // Create bottom wall above the UI — keep particles above GRID_UI_BOUNDARY
  for (int x = 0; x < GRID_WIDTH; x++) {
    grid[GRID_UI_BOUNDARY - 1][x] = Particle::WALL;
    grid[GRID_HEIGHT - 1][x] = Particle::WALL;
  }
  
  // Create side walls
  for (int y = 0; y < GRID_HEIGHT; y++) {
    grid[y][0] = Particle::WALL;
    grid[y][GRID_WIDTH - 1] = Particle::WALL;
  }
}

// Check if coordinates are valid
bool isValid(int x, int y) {
  return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

// Check if a cell is empty (air)
bool isEmpty(int x, int y) {
  return isValid(x, y) && grid[y][x] == Particle::AIR;
}

// Check if a particle can move to a position
bool canMoveTo(int x, int y, Particle type) {
  if (!isValid(x, y)) return false;
  
  Particle target = grid[y][x];
  
  // Air can always be moved into
  if (target == Particle::AIR) return true;
  
  // Sand and Ice can displace water
  if ((type == Particle::SAND || type == Particle::ICE) && target == Particle::WATER) return true;
  
  return false;
}

// Swap two particles
void swap(int x1, int y1, int x2, int y2) {
  Particle temp = grid[y1][x1];
  grid[y1][x1] = grid[y2][x2];
  grid[y2][x2] = temp;
}
