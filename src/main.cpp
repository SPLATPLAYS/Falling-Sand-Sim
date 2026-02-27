#include <appdef.h>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sdk/calc/calc.h>
#include <sdk/os/debug.h>
#include <sdk/os/lcd.h>
#include <sdk/os/gui.h>
#include <sdk/os/input.h>

APP_NAME("Falling Sand")
APP_AUTHOR("SPLATPLAYS")
APP_DESCRIPTION("A falling sand simulator with multiple particle types")
APP_VERSION("1.0.0")

// Screen dimensions
constexpr int SCREEN_WIDTH = 384;
constexpr int SCREEN_HEIGHT = 192;

// Simulation grid dimensions (scaled down for performance)
constexpr int GRID_WIDTH = 192;
constexpr int GRID_HEIGHT = 96;
constexpr int PIXEL_SIZE = 2; // Each cell is 2x2 pixels

// Particle types
enum class Particle : uint8_t {
  AIR = 0,
  SAND,
  WATER,
  STONE,
  WALL
};

// Color definitions (RGB565 format)
constexpr uint16_t COLOR_AIR = 0x0000;     // Black
constexpr uint16_t COLOR_SAND = 0xFDA0;    // Sandy yellow
constexpr uint16_t COLOR_WATER = 0x03BF;   // Blue
constexpr uint16_t COLOR_STONE = 0x7BEF;   // Gray
constexpr uint16_t COLOR_WALL = 0x4208;    // Dark gray

// Brush size
constexpr int BRUSH_SIZE = 3;

// Global grid
Particle grid[GRID_HEIGHT][GRID_WIDTH];
bool updated[GRID_HEIGHT][GRID_WIDTH]; // Track which cells were updated this frame

// Current selected particle
Particle selectedParticle = Particle::SAND;

// Actual LCD dimensions (set at runtime)
int lcdWidth = SCREEN_WIDTH;
int lcdHeight = SCREEN_HEIGHT;

// Get color for particle type
uint16_t getParticleColor(Particle p) {
  switch (p) {
    case Particle::SAND: return COLOR_SAND;
    case Particle::WATER: return COLOR_WATER;
    case Particle::STONE: return COLOR_STONE;
    case Particle::WALL: return COLOR_WALL;
    default: return COLOR_AIR;
  }
}

// Initialize the grid
void initGrid() {
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      grid[y][x] = Particle::AIR;
      updated[y][x] = false;
    }
  }
  
  // Create bottom wall above the UI (UI starts at Y=176 pixels = grid row 88)
  // Create wall at row 87 to keep particles above UI
  const int UI_BOUNDARY = (SCREEN_HEIGHT - 16) / PIXEL_SIZE;
  for (int x = 0; x < GRID_WIDTH; x++) {
    grid[UI_BOUNDARY - 1][x] = Particle::WALL;
    grid[GRID_HEIGHT - 1][x] = Particle::WALL;
  }
  
  // Create side walls
  for (int y = 0; y < GRID_HEIGHT; y++) {
    grid[y][0] = Particle::WALL;
    grid[y][GRID_WIDTH - 1] = Particle::WALL;
  }
}

// Check if coordinates are valid
inline bool isValid(int x, int y) {
  return x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT;
}

// Check if a cell is empty (air)
inline bool isEmpty(int x, int y) {
  return isValid(x, y) && grid[y][x] == Particle::AIR;
}

// Check if a particle can move to a position
inline bool canMoveTo(int x, int y, Particle type) {
  if (!isValid(x, y)) return false;
  
  Particle target = grid[y][x];
  
  // Air can always be moved into
  if (target == Particle::AIR) return true;
  
  // Sand can displace water
  if (type == Particle::SAND && target == Particle::WATER) return true;
  
  return false;
}

// Swap two particles
inline void swap(int x1, int y1, int x2, int y2) {
  Particle temp = grid[y1][x1];
  grid[y1][x1] = grid[y2][x2];
  grid[y2][x2] = temp;
}

// Update sand particle
void updateSand(int x, int y) {
  // Try to fall straight down
  if (canMoveTo(x, y + 1, Particle::SAND)) {
    swap(x, y, x, y + 1);
    updated[y + 1][x] = true;
  }
  // Try diagonal down-left
  else if (canMoveTo(x - 1, y + 1, Particle::SAND)) {
    swap(x, y, x - 1, y + 1);
    updated[y + 1][x - 1] = true;
  }
  // Try diagonal down-right
  else if (canMoveTo(x + 1, y + 1, Particle::SAND)) {
    swap(x, y, x + 1, y + 1);
    updated[y + 1][x + 1] = true;
  }
}

// Update water particle
void updateWater(int x, int y) {
  // Try to fall straight down (only into empty space)
  if (isEmpty(x, y + 1)) {
    swap(x, y, x, y + 1);
    updated[y + 1][x] = true;
  }
  // Try diagonal down-left (only into empty space)
  else if (isEmpty(x - 1, y + 1)) {
    swap(x, y, x - 1, y + 1);
    updated[y + 1][x - 1] = true;
  }
  // Try diagonal down-right (only into empty space)
  else if (isEmpty(x + 1, y + 1)) {
    swap(x, y, x + 1, y + 1);
    updated[y + 1][x + 1] = true;
  }
  // Try to flow sideways - randomize direction for balanced spreading
  else {
    bool tryLeftFirst = (rand() % 2) == 0;
    int dir1 = tryLeftFirst ? -1 : 1;
    int dir2 = tryLeftFirst ? 1 : -1;
    
    if (isEmpty(x + dir1, y)) {
      swap(x, y, x + dir1, y);
      updated[y][x + dir1] = true;
    }
    else if (isEmpty(x + dir2, y)) {
      swap(x, y, x + dir2, y);
      updated[y][x + dir2] = true;
    }
  }
}

// Update stone particle (just falls, no sideways movement)
void updateStone(int x, int y) {
  if (isEmpty(x, y + 1)) {
    swap(x, y, x, y + 1);
    updated[y + 1][x] = true;
  }
}

// Simulate one step
void simulate() {
  // Clear update flags
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      updated[y][x] = false;
    }
  }
  
  // Update from bottom to top, randomizing left-right order
  for (int y = GRID_HEIGHT - 2; y >= 0; y--) {
    // Alternate scan direction for more natural behavior
    bool scanLeft = (y % 2) == 0;
    
    for (int i = 0; i < GRID_WIDTH; i++) {
      int x = scanLeft ? i : (GRID_WIDTH - 1 - i);
      
      // Skip if already updated this frame
      if (updated[y][x]) continue;
      
      Particle p = grid[y][x];
      
      switch (p) {
        case Particle::SAND:
          updateSand(x, y);
          break;
        case Particle::WATER:
          updateWater(x, y);
          break;
        case Particle::STONE:
          updateStone(x, y);
          break;
        default:
          break;
      }
    }
  }
}

// Draw the grid to screen
void drawGrid(uint16_t* vram) {
  for (int y = 0; y < GRID_HEIGHT; y++) {
    for (int x = 0; x < GRID_WIDTH; x++) {
      uint16_t color = getParticleColor(grid[y][x]);
      
      // Draw PIXEL_SIZE x PIXEL_SIZE block
      for (int dy = 0; dy < PIXEL_SIZE; dy++) {
        for (int dx = 0; dx < PIXEL_SIZE; dx++) {
          int screenX = x * PIXEL_SIZE + dx;
          int screenY = y * PIXEL_SIZE + dy;
          if (screenX < lcdWidth && screenY < lcdHeight) {
            vram[screenY * lcdWidth + screenX] = color;
          }
        }
      }
    }
  }
  
  // Draw UI - particle selector at bottom
  const int UI_Y = SCREEN_HEIGHT - 16;
  const int SWATCH_SIZE = 16;
  const int SWATCH_SPACING = 20;
  const int UI_START_X = 10;
  
  Particle particles[] = {Particle::SAND, Particle::WATER, Particle::STONE, Particle::WALL, Particle::AIR};
  
  for (int i = 0; i < 5; i++) {
    uint16_t color = getParticleColor(particles[i]);
    // Use bright pink for AIR in UI so it's visible
    if (particles[i] == Particle::AIR) {
      color = 0xF81F; // Bright pink (magenta)
    }
    int x = UI_START_X + i * SWATCH_SPACING;
    
    // Draw swatch
    for (int dy = 0; dy < SWATCH_SIZE; dy++) {
      for (int dx = 0; dx < SWATCH_SIZE; dx++) {
        vram[(UI_Y + dy) * lcdWidth + (x + dx)] = color;
      }
    }
    
    // Highlight selected particle
    if (particles[i] == selectedParticle) {
      uint16_t highlight = 0xFFFF; // White
      // Draw border
      for (int dx = 0; dx < SWATCH_SIZE; dx++) {
        vram[UI_Y * lcdWidth + (x + dx)] = highlight;
        vram[(UI_Y + SWATCH_SIZE - 1) * lcdWidth + (x + dx)] = highlight;
      }
      for (int dy = 0; dy < SWATCH_SIZE; dy++) {
        vram[(UI_Y + dy) * lcdWidth + x] = highlight;
        vram[(UI_Y + dy) * lcdWidth + (x + SWATCH_SIZE - 1)] = highlight;
      }
    }
  }
}

// Place particles at position
void placeParticle(int gridX, int gridY) {
  if (!isValid(gridX, gridY)) return;
  
  // Calculate UI boundary (UI starts at Y=176 pixels = grid row 88)
  const int UI_BOUNDARY = (SCREEN_HEIGHT - 16) / PIXEL_SIZE;
  
  // Don't allow placing anything at or below the UI boundary
  if (gridY >= UI_BOUNDARY) return;
  
  // If erasing (placing AIR), allow erasing anything except the UI boundary wall
  if (selectedParticle == Particle::AIR) {
    for (int dy = -BRUSH_SIZE/2; dy <= BRUSH_SIZE/2; dy++) {
      for (int dx = -BRUSH_SIZE/2; dx <= BRUSH_SIZE/2; dx++) {
        int x = gridX + dx;
        int y = gridY + dy;
        if (isValid(x, y) && y < UI_BOUNDARY) {
          // Don't erase the UI boundary wall at row 87
          if (y == UI_BOUNDARY - 1) continue;
          grid[y][x] = Particle::AIR;
        }
      }
    }
    return;
  }
  
  // Don't overwrite walls unless placing wall
  if (grid[gridY][gridX] == Particle::WALL && selectedParticle != Particle::WALL) {
    return;
  }
  
  // Place particles in brush area
  for (int dy = -BRUSH_SIZE/2; dy <= BRUSH_SIZE/2; dy++) {
    for (int dx = -BRUSH_SIZE/2; dx <= BRUSH_SIZE/2; dx++) {
      int x = gridX + dx;
      int y = gridY + dy;
      if (isValid(x, y) && y < UI_BOUNDARY) {
        // Skip walls unless placing wall
        if (grid[y][x] == Particle::WALL && selectedParticle != Particle::WALL) {
          continue;
        }
        grid[y][x] = selectedParticle;
      }
    }
  }
}

// Handle input, returns true if should exit
bool handleInput() {
  struct Input_Event event;
  memset(&event, 0, sizeof(event));
  
  // Poll for events (non-blocking)
  while (GetInput(&event, 0, 0x10) == 0 && event.type != EVENT_NONE) {
    if (event.type == EVENT_TOUCH) {
      int touchX = event.data.touch_single.p1_x;
      int touchY = event.data.touch_single.p1_y;
      
      // Check if touching UI area (particle selector)
      // Only treat as UI if touching the actual swatches
      bool touchedUI = false;
      if (touchY >= SCREEN_HEIGHT - 16) {
        const int UI_START_X = 10;
        const int SWATCH_SIZE = 16;
        const int SWATCH_SPACING = 20;
        
        Particle particles[] = {Particle::SAND, Particle::WATER, Particle::STONE, Particle::WALL, Particle::AIR};
        
        for (int j = 0; j < 5; j++) {
          int x = UI_START_X + j * SWATCH_SPACING;
          if (touchX >= x && touchX < x + SWATCH_SIZE && touchY >= SCREEN_HEIGHT - 16 && touchY < SCREEN_HEIGHT) {
            selectedParticle = particles[j];
            touchedUI = true;
            break;
          }
        }
      }
      
      // Place particles on grid if not touching UI
      if (!touchedUI) {
        int gridX = touchX / PIXEL_SIZE;
        int gridY = touchY / PIXEL_SIZE;
        placeParticle(gridX, gridY);
      }
    } else if (event.type == EVENT_KEY) {
      // Clear screen with CLEAR key 
      if (event.data.key.keyCode == KEYCODE_POWER_CLEAR && 
          event.data.key.direction == KEY_PRESSED) {
        initGrid();
      }
      // Exit with EXE key
      if (event.data.key.keyCode == KEYCODE_EXE && 
          event.data.key.direction == KEY_PRESSED) {
        return true;
      }
    } else if (event.type == EVENT_ACTBAR_ESC) {
      return true;
    }
    
    // Reset for next iteration
    memset(&event, 0, sizeof(event));
  }
  return false;
}

int main(int argc, char **argv, char **envp) {
  (void)argc;
  (void)argv;
  (void)envp;
  
  // Initialize random seed
  srand(0x12345678);
  
  // Initialize grid
  initGrid();
  
  // Get actual LCD dimensions
  unsigned int width, height;
  LCD_GetSize(&width, &height);
  lcdWidth = width;
  lcdHeight = height;
  
  // Main loop
  bool running = true;
  
  while (running) {
    // Get VRAM directly - using direct VRAM access for speed
    uint16_t *vramPtr = (uint16_t*)LCD_GetVRAMAddress();
    
    // Simulate physics every frame
    simulate();
    
    // Draw to VRAM
    drawGrid(vramPtr);
    
    // Handle input and check for exit
    if (handleInput()) {
      running = false;
    }
    
    // Refresh display to push VRAM contents to LCD
    // This call blocks until the display is refreshed, providing natural frame pacing
    LCD_Refresh();
  }
  
  return 0;
}