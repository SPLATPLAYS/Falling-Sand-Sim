#include "input.h"
#include "config.h"
#include "grid.h"
#include "particle.h"
#include <cstring>
#include <sdk/os/input.h>

// Current selected particle
Particle selectedParticle = Particle::SAND;

// Place particles at position
static void placeParticle(int gridX, int gridY) {
  if (!isValid(gridX, gridY)) return;
  
  // Calculate UI boundary (UI starts at Y=176 pixels = grid row 88)
  const int UI_BOUNDARY = (SCREEN_HEIGHT - UI_HEIGHT) / PIXEL_SIZE;
  
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
          temperature[y][x] = TEMP_AMBIENT;
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
        temperature[y][x] = getParticleTemperature(selectedParticle);
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
      if (touchY >= SCREEN_HEIGHT - UI_HEIGHT) {
        Particle particles[] = {Particle::SAND, Particle::WATER, Particle::STONE, Particle::WALL, Particle::LAVA, Particle::PLANT, Particle::AIR};
        
        for (int j = 0; j < PARTICLE_TYPE_COUNT; j++) {
          int x = UI_START_X + j * SWATCH_SPACING;
          if (touchX >= x && touchX < x + SWATCH_SIZE && touchY >= SCREEN_HEIGHT - UI_HEIGHT && touchY < SCREEN_HEIGHT) {
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
