#include "grid.h"

// Global grid - aligned for better cache performance
alignas(32) Particle grid[GRID_HEIGHT][GRID_WIDTH];
alignas(32) bool updated[GRID_HEIGHT][GRID_WIDTH]; // Track which cells were updated this frame
alignas(32) uint8_t temperature[GRID_HEIGHT][GRID_WIDTH]; // Temperature value for each cell (0-255)

// Initialize the grid
void initGrid() {
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      grid[y][x] = Particle::AIR;
      updated[y][x] = false;
      temperature[y][x] = TEMP_AMBIENT; // Initialize to ambient temperature
    }
  }
  
  // Create bottom wall above the UI (UI starts at Y=176 pixels = grid row 88)
  // Create wall at row 87 to keep particles above UI
  const int UI_BOUNDARY = (SCREEN_HEIGHT - UI_HEIGHT) / PIXEL_SIZE;
  for (int x = 0; x < GRID_WIDTH; x++) {
    grid[UI_BOUNDARY - 1][x] = Particle::WALL;
    temperature[UI_BOUNDARY - 1][x] = TEMP_AMBIENT;
    grid[GRID_HEIGHT - 1][x] = Particle::WALL;
    temperature[GRID_HEIGHT - 1][x] = TEMP_AMBIENT;
  }
  
  // Create side walls
  for (int y = 0; y < GRID_HEIGHT; y++) {
    grid[y][0] = Particle::WALL;
    temperature[y][0] = TEMP_AMBIENT;
    grid[y][GRID_WIDTH - 1] = Particle::WALL;
    temperature[y][GRID_WIDTH - 1] = TEMP_AMBIENT;
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
  
  // Sand can displace water
  if (type == Particle::SAND && target == Particle::WATER) return true;
  
  return false;
}

// Swap two particles
void swap(int x1, int y1, int x2, int y2) {
  Particle temp = grid[y1][x1];
  grid[y1][x1] = grid[y2][x2];
  grid[y2][x2] = temp;
}
