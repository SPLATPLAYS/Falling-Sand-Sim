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

#include "config.h"
#include "particle.h"
#include "random.h"
#include "grid.h"
#include "physics.h"
#include "renderer.h"
#include "input.h"
#include "settings.h"
#include "overclock.h"

APP_NAME("Falling Sand")
APP_AUTHOR("SPLATPLAYS")
APP_DESCRIPTION("A falling sand simulator with multiple particle types")
APP_VERSION("1.0.0")

int main(int argc, char **argv, char **envp) {
  (void)argc;
  (void)argv;
  (void)envp;
  
  // XorShift state is already initialized globally
  
  // Snapshot the OS-default CPG clock registers FIRST, before any other
  // hardware init that might change clock state.
  oclock_init();

  // Initialize grid
  initGrid();

  // Load persisted settings (brush size + overclock level) from MCS
  initSettings();

  // Apply the persisted overclock level (level 0 = default = no register write).
  oclock_apply(overclockLevel);

  // Get actual LCD dimensions and initialize renderer
  unsigned int width, height;
  LCD_GetSize(&width, &height);
  initRenderer(width, height);

  // Outer loop — re-enters the start menu after returning from gameplay.
  // The only way to exit fully is via the EXIT option in the start menu.
  bool appRunning = true;
  while (appRunning) {

    // --- Start menu ---
    bool inMenu = true;
    while (inMenu) {
      uint16_t *vramPtr = (uint16_t*)LCD_GetVRAMAddress();
      drawStartMenu(vramPtr);
      LCD_Refresh();
      int result = handleStartMenuInput();
      if (result == 1) {
        // Clear to black before game starts
        uint16_t *vramPtr2 = (uint16_t*)LCD_GetVRAMAddress();
        for (int i = 0; i < (int)(width * height); i++) vramPtr2[i] = 0x0000;
        LCD_Refresh();
        inMenu = false;
      } else if (result == 2) {
        // --- Top-level settings menu ---
        int settingsRow = 0;
        bool inSettings = true;
        while (inSettings) {
          uint16_t *sv = (uint16_t*)LCD_GetVRAMAddress();
          drawSettingsMenu(sv, settingsRow);
          LCD_Refresh();
          int sr = handleSettingsMenuInput(settingsRow);
          if (sr == 1) {
            // EXE: enter the highlighted sub-menu
            if (settingsRow == 0) {
              // --- CPU speed (overclock) sub-menu ---
              int pendingLevel = overclockLevel;
              bool inOC = true;
              while (inOC) {
                uint16_t *ov = (uint16_t*)LCD_GetVRAMAddress();
                drawOCScreen(ov, pendingLevel);
                LCD_Refresh();
                int ocr = handleOCInput(pendingLevel);
                if (ocr == 1) {
                  overclockLevel = pendingLevel;
                  oclock_apply(overclockLevel);
                  saveOverclockLevel();
                  inOC = false;
                } else if (ocr == -1) {
                  oclock_apply(overclockLevel); // restore
                  inOC = false;
                }
              }
            } else if (settingsRow == 1) {
              // --- Simulation speed sub-menu ---
              int pendingMode = simSpeedMode;
              bool inSim = true;
              while (inSim) {
                uint16_t *simv = (uint16_t*)LCD_GetVRAMAddress();
                drawSimSpeedScreen(simv, pendingMode);
                LCD_Refresh();
                int simr = handleSimSpeedInput(pendingMode);
                if (simr == 1) {
                  simSpeedMode = pendingMode;
                  saveSimSpeedMode();
                  inSim = false;
                } else if (simr == -1) {
                  inSim = false; // discard pending change
                }
              }
            }
          } else if (sr == -1) {
            inSettings = false;
          }
        }
      } else if (result == -1) {
        appRunning = false; // EXIT chosen in start menu — quit app
        inMenu = false;
      }
    }

    if (!appRunning) break;

    // --- Game loop ---
    // EXE / activity-bar ESC returns to the start menu (handled in handleInput).
    bool running = true;
    uint32_t frameCount = 0;

    while (running) {
      // Simulate physics every frame (always run, regardless of frame skip)
      simulate();

      // Track physics FPS every frame
      updateFPS();

      // Determine if we should render this frame based on the runtime sim speed mode.
      // simSkipAmounts[0]=0 means always render; higher modes skip N frames between renders.
      bool shouldRender = true;
      int skipAmt = simSkipAmounts[simSpeedMode];
      if (skipAmt > 0) {
        shouldRender = (frameCount % (skipAmt + 1)) == 0;
      }

      if (shouldRender) {
        uint16_t *vramPtr = (uint16_t*)LCD_GetVRAMAddress();
        drawGrid(vramPtr);
        LCD_Refresh();
      }

      // Handle input every frame (even if not rendering).
      // Returns true when the player wants to return to the main menu.
      if (handleInput()) {
        running = false;
      }

      frameCount++;
    }
    // Falls through to restart the start menu.
  }

  return 0;
}