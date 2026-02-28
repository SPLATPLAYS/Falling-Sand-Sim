#include "renderer.h"
#include "config.h"
#include "particle.h"
#include "grid.h"
#include <ctime>

// Actual LCD dimensions (set at runtime)
int lcdWidth = SCREEN_WIDTH;
int lcdHeight = SCREEN_HEIGHT;

// FPS tracking
static uint32_t frameTimes[FPS_SAMPLE_COUNT] = {0};
static int frameIndex = 0;
static uint32_t lastFrameTime = 0;
static float currentFPS = 0.0f;

// External reference to selected particle (from input.cpp)
extern Particle selectedParticle;

// Simple cycle counter for timing (using clock())
static uint32_t getTickCount() {
  return static_cast<uint32_t>(clock());
}

// Initialize renderer with LCD dimensions
void initRenderer(int width, int height) {
  lcdWidth = width;
  lcdHeight = height;
  
  // Initialize FPS tracking
  lastFrameTime = getTickCount();
  for (int i = 0; i < FPS_SAMPLE_COUNT; i++) {
    frameTimes[i] = CLOCKS_PER_SEC / 60; // Assume 60 FPS initially
  }
}

// Update FPS counter
void updateFPS() {
  uint32_t currentTime = getTickCount();
  uint32_t delta = currentTime - lastFrameTime;
  lastFrameTime = currentTime;
  
  // Store frame time
  frameTimes[frameIndex] = delta;
  frameIndex = (frameIndex + 1) % FPS_SAMPLE_COUNT;
  
  // Calculate average frame time
  uint32_t totalTime = 0;
  for (int i = 0; i < FPS_SAMPLE_COUNT; i++) {
    totalTime += frameTimes[i];
  }
  
  if (totalTime > 0) {
    // FPS = (samples * CLOCKS_PER_SEC) / total_time
    currentFPS = (static_cast<float>(FPS_SAMPLE_COUNT) * CLOCKS_PER_SEC) / totalTime;
  }
}

// Draw a 5x7 digit to VRAM
static void drawDigit(uint16_t* vram, int x, int y, int digit, uint16_t color) {
  // 5x7 font for digits 0-9
  static const uint8_t font[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, // 0
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}, // 1
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, // 2
    {0x0E, 0x11, 0x01, 0x0E, 0x01, 0x11, 0x0E}, // 3
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, // 4
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}, // 5
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}, // 6
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, // 8
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}  // 9
  };
  
  if (digit < 0 || digit > 9) return;
  
  for (int row = 0; row < 7; row++) {
    uint8_t rowData = font[digit][row];
    for (int col = 0; col < 5; col++) {
      if (rowData & (1 << (4 - col))) {
        int px = x + col;
        int py = y + row;
        if (px >= 0 && px < lcdWidth && py >= 0 && py < lcdHeight) {
          vram[py * lcdWidth + px] = color;
        }
      }
    }
  }
}

// Draw FPS counter on screen
static void drawFPS(uint16_t* vram) {
  int fps = static_cast<int>(currentFPS + 0.5f);
  if (fps > 999) fps = 999;
  
  // Extract digits
  int hundreds = fps / 100;
  int tens = (fps / 10) % 10;
  int ones = fps % 10;
  
  int x = FPS_DISPLAY_X;
  
  // Draw digits with spacing
  if (hundreds > 0) {
    drawDigit(vram, x, FPS_DISPLAY_Y, hundreds, COLOR_HIGHLIGHT);
    x += 6;
  }
  if (hundreds > 0 || tens > 0) {
    drawDigit(vram, x, FPS_DISPLAY_Y, tens, COLOR_HIGHLIGHT);
    x += 6;
  }
  drawDigit(vram, x, FPS_DISPLAY_Y, ones, COLOR_HIGHLIGHT);
}

// Draw the grid to screen - optimized for faster VRAM writes
void drawGrid(uint16_t* vram) {
  // Optimized: write scanlines directly with proper bounds checking
  // Each grid cell is PIXEL_SIZE x PIXEL_SIZE pixels
  for (int y = 0; y < GRID_HEIGHT; y++) {
    int screenY = y * PIXEL_SIZE;
    // Calculate base VRAM addresses for both scanlines of this grid row
    uint16_t* scanline0 = vram + screenY * lcdWidth;
    uint16_t* scanline1 = vram + (screenY + 1) * lcdWidth;
    
    for (int x = 0; x < GRID_WIDTH; x++) {
      uint16_t color = getParticleColor(grid[y][x]);
      
      // Write 2x2 pixel block using direct pointer writes
      int screenX = x * PIXEL_SIZE;
      if (screenX + 1 < lcdWidth && screenY + 1 < lcdHeight) {
        scanline0[screenX] = color;
        scanline0[screenX + 1] = color;
        scanline1[screenX] = color;
        scanline1[screenX + 1] = color;
      }
    }
  }
  
  // Draw UI - particle selector at bottom
  const int UI_Y = SCREEN_HEIGHT - UI_HEIGHT;
  
  Particle particles[] = {Particle::SAND, Particle::WATER, Particle::STONE, Particle::WALL, Particle::LAVA, Particle::PLANT, Particle::AIR};
  
  for (int i = 0; i < PARTICLE_TYPE_COUNT; i++) {
    uint16_t color = getParticleColor(particles[i]);
    // Use bright pink for AIR in UI so it's visible
    if (particles[i] == Particle::AIR) {
      color = COLOR_UI_AIR;
    }
    int x = UI_START_X + i * SWATCH_SPACING;
    
    // Draw swatch using scanline pointers for efficiency
    for (int dy = 0; dy < SWATCH_SIZE; dy++) {
      uint16_t* scanline = vram + (UI_Y + dy) * lcdWidth + x;
      // Write 16 pixels at once using pointer writes
      for (int dx = 0; dx < SWATCH_SIZE; dx++) {
        scanline[dx] = color;
      }
    }
    
    // Highlight selected particle
    if (particles[i] == selectedParticle) {
      // Draw border using direct scanline writes
      uint16_t* topBorder = vram + UI_Y * lcdWidth + x;
      uint16_t* bottomBorder = vram + (UI_Y + SWATCH_SIZE - 1) * lcdWidth + x;
      for (int dx = 0; dx < SWATCH_SIZE; dx++) {
        topBorder[dx] = COLOR_HIGHLIGHT;
        bottomBorder[dx] = COLOR_HIGHLIGHT;
      }
      for (int dy = 0; dy < SWATCH_SIZE; dy++) {
        uint16_t* scanline = vram + (UI_Y + dy) * lcdWidth;
        scanline[x] = COLOR_HIGHLIGHT;
        scanline[x + SWATCH_SIZE - 1] = COLOR_HIGHLIGHT;
      }
    }
  }
  
  // Draw FPS counter
  drawFPS(vram);
}
