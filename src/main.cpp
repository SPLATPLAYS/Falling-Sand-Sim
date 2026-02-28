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

APP_NAME("Falling Sand")
APP_AUTHOR("SPLATPLAYS")
APP_DESCRIPTION("A falling sand simulator with multiple particle types")
APP_VERSION("1.0.0")

int main(int argc, char **argv, char **envp) {
  (void)argc;
  (void)argv;
  (void)envp;
  
  // XorShift state is already initialized globally
  
  // Initialize grid
  initGrid();
  
  // Get actual LCD dimensions and initialize renderer
  unsigned int width, height;
  LCD_GetSize(&width, &height);
  initRenderer(width, height);
  
  // Main loop
  bool running = true;
  uint32_t frameCount = 0;
  
  while (running) {
    // Simulate physics every frame (always run, regardless of frame skip)
    simulate();
    
    // Determine if we should render this frame
    bool shouldRender = true;
    if (FRAME_SKIP_ENABLED && FRAME_SKIP_AMOUNT > 0) {
      shouldRender = (frameCount % (FRAME_SKIP_AMOUNT + 1)) == 0;
    }
    
    if (shouldRender) {
      // Update FPS counter only on rendered frames
      updateFPS();
      
      // Get VRAM directly - using direct VRAM access for speed
      uint16_t *vramPtr = (uint16_t*)LCD_GetVRAMAddress();
      
      // Draw to VRAM
      drawGrid(vramPtr);
      
      // Refresh display to push VRAM contents to LCD
      // This call blocks until the display is refreshed, providing natural frame pacing
      LCD_Refresh();
    }
    
    // Handle input every frame (even if not rendering)
    if (handleInput()) {
      running = false;
    }
    
    frameCount++;
  }
  
  return 0;
}