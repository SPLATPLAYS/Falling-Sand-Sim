#include "input.h"
#include "config.h"
#include "grid.h"
#include "particle.h"
#include "overclock.h"
#include "renderer.h"
#include "settings.h"
#include <cstring>
#include <sdk/os/input.h>

// Current selected particle
Particle selectedParticle = Particle::SAND;

// Current brush size (runtime, persisted in MCS)
int brushSize = BRUSH_SIZE_DEFAULT;

// Temperature heat-map overlay toggle
bool tempViewEnabled = false;

// Place particles at position
static void placeParticle(int gridX, int gridY) {
  if (!isValid(gridX, gridY)) return;
  
  // Don't allow placing anything at or below the UI boundary
  if (gridY >= GRID_UI_BOUNDARY) return;
  
  // If erasing (placing AIR), allow erasing anything except the UI boundary wall
  if (selectedParticle == Particle::AIR) {
    for (int dy = -brushSize/2; dy <= brushSize/2; dy++) {
      for (int dx = -brushSize/2; dx <= brushSize/2; dx++) {
        int x = gridX + dx;
        int y = gridY + dy;
        if (isValid(x, y) && y < GRID_UI_BOUNDARY) {
          // Don't erase the UI boundary wall at row GRID_UI_BOUNDARY-1
          if (y == GRID_UI_BOUNDARY - 1) continue;
          grid[y][x] = Particle::AIR;
          tempSet(x, y, TEMP_AMBIENT);
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
  for (int dy = -brushSize/2; dy <= brushSize/2; dy++) {
    for (int dx = -brushSize/2; dx <= brushSize/2; dx++) {
      int x = gridX + dx;
      int y = gridY + dy;
      if (isValid(x, y) && y < GRID_UI_BOUNDARY) {
        // Skip walls unless placing wall
        if (grid[y][x] == Particle::WALL && selectedParticle != Particle::WALL) {
          continue;
        }
        grid[y][x] = selectedParticle;
        tempSet(x, y, getParticleTemperature(selectedParticle));
      }
    }
  }
}

// Handle start-menu input.
// Returns 1=Play, 2=Settings, -1=Exit, 0=nothing yet.
int handleStartMenuInput() {
  struct Input_Event event;
  memset(&event, 0, sizeof(event));

  while (GetInput(&event, 0, 0x10) == 0 && event.type != EVENT_NONE) {
    if (event.type == EVENT_TOUCH) {
      int tx = event.data.touch_single.p1_x;
      int ty = event.data.touch_single.p1_y;

      auto hit = [&](int bx, int by, int bw, int bh) {
        return tx >= bx && tx < bx + bw && ty >= by && ty < by + bh;
      };

      if (hit(startMenuPlayBtnX,     startMenuPlayBtnY,     startMenuPlayBtnW,     startMenuPlayBtnH))     return 1;
      if (hit(startMenuSettingsBtnX, startMenuSettingsBtnY, startMenuSettingsBtnW, startMenuSettingsBtnH)) return 2;
      if (hit(startMenuExitBtnX,     startMenuExitBtnY,     startMenuExitBtnW,     startMenuExitBtnH))     return -1;
    } else if (event.type == EVENT_KEY) {
      if (event.data.key.direction == KEY_PRESSED) {
        if (event.data.key.keyCode == KEYCODE_EXE)          return 1;
        if (event.data.key.keyCode == KEYCODE_POWER_CLEAR)  return -1;
      }
    } else if (event.type == EVENT_ACTBAR_ESC) {
      return -1;
    }
    memset(&event, 0, sizeof(event));
  }
  return 0;
}

// Handle top-level settings menu input.
// Returns 1=EXE (enter sub-menu), -1=CLEAR/ESC (back), 0=navigating.
int handleSettingsMenuInput(int& selectedItem) {
  struct Input_Event event;
  memset(&event, 0, sizeof(event));

  while (GetInput(&event, 0, 0x10) == 0 && event.type != EVENT_NONE) {
    if (event.type == EVENT_KEY) {
      if (event.data.key.direction == KEY_PRESSED ||
          event.data.key.direction == KEY_HELD) {
        switch (event.data.key.keyCode) {
          case KEYCODE_UP:
            if (selectedItem > 0) selectedItem--;
            break;
          case KEYCODE_DOWN:
            if (selectedItem < 1) selectedItem++;
            break;
          case KEYCODE_EXE:
            if (event.data.key.direction == KEY_PRESSED) {
              memset(&event, 0, sizeof(event));
              return 1;
            }
            break;
          case KEYCODE_POWER_CLEAR:
            if (event.data.key.direction == KEY_PRESSED) {
              memset(&event, 0, sizeof(event));
              return -1;
            }
            break;
          default: break;
        }
      }
    } else if (event.type == EVENT_ACTBAR_ESC) {
      memset(&event, 0, sizeof(event));
      return -1;
    }
    memset(&event, 0, sizeof(event));
  }
  return 0;
}

// Handle overclock sub-menu input.
// Returns 1=confirmed, -1=cancelled, 0=navigating.
int handleOCInput(int& selectedLevel) {
  struct Input_Event event;
  memset(&event, 0, sizeof(event));

  while (GetInput(&event, 0, 0x10) == 0 && event.type != EVENT_NONE) {
    if (event.type == EVENT_KEY) {
      if (event.data.key.direction == KEY_PRESSED ||
          event.data.key.direction == KEY_HELD) {
        switch (event.data.key.keyCode) {
          case KEYCODE_UP:
            if (selectedLevel > OC_LEVEL_MIN) {
              selectedLevel--;
              oclock_apply(selectedLevel); // live preview
            }
            break;
          case KEYCODE_DOWN:
            if (selectedLevel < OC_LEVEL_MAX) {
              selectedLevel++;
              oclock_apply(selectedLevel);
            }
            break;
          case KEYCODE_EXE:
            if (event.data.key.direction == KEY_PRESSED) {
              memset(&event, 0, sizeof(event));
              return 1;
            }
            break;
          case KEYCODE_POWER_CLEAR:
            if (event.data.key.direction == KEY_PRESSED) {
              memset(&event, 0, sizeof(event));
              return -1;
            }
            break;
          default: break;
        }
      }
    } else if (event.type == EVENT_ACTBAR_ESC) {
      memset(&event, 0, sizeof(event));
      return -1;
    }
    memset(&event, 0, sizeof(event));
  }
  return 0;
}

// Handle simulation speed sub-menu input.
// Returns 1=confirmed, -1=cancelled, 0=navigating.
int handleSimSpeedInput(int& selectedMode) {
  struct Input_Event event;
  memset(&event, 0, sizeof(event));

  while (GetInput(&event, 0, 0x10) == 0 && event.type != EVENT_NONE) {
    if (event.type == EVENT_KEY) {
      if (event.data.key.direction == KEY_PRESSED ||
          event.data.key.direction == KEY_HELD) {
        switch (event.data.key.keyCode) {
          case KEYCODE_UP:
            if (selectedMode > 0) selectedMode--;
            break;
          case KEYCODE_DOWN:
            if (selectedMode < SIM_SPEED_MODE_MAX) selectedMode++;
            break;
          case KEYCODE_EXE:
            if (event.data.key.direction == KEY_PRESSED) {
              memset(&event, 0, sizeof(event));
              return 1;
            }
            break;
          case KEYCODE_POWER_CLEAR:
            if (event.data.key.direction == KEY_PRESSED) {
              memset(&event, 0, sizeof(event));
              return -1;
            }
            break;
          default: break;
        }
      }
    } else if (event.type == EVENT_ACTBAR_ESC) {
      memset(&event, 0, sizeof(event));
      return -1;
    }
    memset(&event, 0, sizeof(event));
  }
  return 0;
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
      
      // Check if touching UI area (particle selector or brush slider)
      bool touchedUI = false;
      if (touchY >= SCREEN_HEIGHT - UI_HEIGHT) {
        // Check particle swatches
        for (int j = 0; j < PARTICLE_TYPE_COUNT; j++) {
          int x = UI_START_X + j * SWATCH_SPACING;
          if (touchX >= x && touchX < x + SWATCH_SIZE) {
            selectedParticle = PARTICLE_UI_ORDER[j];
            touchedUI = true;
            break;
          }
        }
        // Check brush size slider track area
        if (!touchedUI &&
            touchX >= BRUSH_SLIDER_TRACK_X &&
            touchX < BRUSH_SLIDER_TRACK_X + BRUSH_SLIDER_TRACK_W) {
          // Map X position to brush size
          int rel = touchX - BRUSH_SLIDER_TRACK_X;
          int newSize = BRUSH_SIZE_MIN +
            rel * (BRUSH_SIZE_MAX - BRUSH_SIZE_MIN) / (BRUSH_SLIDER_TRACK_W - 1);
          if (newSize < BRUSH_SIZE_MIN) newSize = BRUSH_SIZE_MIN;
          if (newSize > BRUSH_SIZE_MAX) newSize = BRUSH_SIZE_MAX;
          if (newSize != brushSize) {
            brushSize = newSize;
            saveBrushSize();
          }
          touchedUI = true;
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
      // + key: increase brush size
      if (event.data.key.keyCode == KEYCODE_PLUS &&
          (event.data.key.direction == KEY_PRESSED ||
           event.data.key.direction == KEY_HELD)) {
        if (brushSize < BRUSH_SIZE_MAX) {
          brushSize++;
          if (event.data.key.direction == KEY_PRESSED) saveBrushSize();
        }
      }
      // - key: decrease brush size
      if (event.data.key.keyCode == KEYCODE_MINUS &&
          (event.data.key.direction == KEY_PRESSED ||
           event.data.key.direction == KEY_HELD)) {
        if (brushSize > BRUSH_SIZE_MIN) {
          brushSize--;
          if (event.data.key.direction == KEY_PRESSED) saveBrushSize();
        }
      }
      // 0 key: toggle temperature heat-map overlay
      if (event.data.key.keyCode == KEYCODE_0 &&
          event.data.key.direction == KEY_PRESSED) {
        tempViewEnabled = !tempViewEnabled;
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
