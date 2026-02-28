#include "renderer.h"
#include "config.h"
#include "particle.h"
#include "grid.h"
#include "input.h"
#include <sys/time.h>
#include <cstring>

// Actual LCD dimensions (set at runtime)
int lcdWidth = SCREEN_WIDTH;
int lcdHeight = SCREEN_HEIGHT;

// FPS tracking
static uint32_t frameTimes[FPS_SAMPLE_COUNT] = {0};
static int frameIndex = 0;
static uint32_t lastFrameTime = 0;
static float currentFPS = 0.0f;

// External reference to selected particle (from input.cpp) - now via input.h

// Return current time in microseconds.
// gettimeofday() is backed by TMU2 at Phi/16 on the SH7305, giving
// sub-microsecond resolution — far more precise than clock().
static uint32_t getMicros() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  // Cast seconds to uint32_t before multiplying to avoid 32-bit overflow
  // (tv_sec wraps every ~4295 s ≈ 71 min, which is fine for delta timing).
  return static_cast<uint32_t>(tv.tv_sec) * 1000000u
       + static_cast<uint32_t>(tv.tv_usec);
}

// Initialize renderer with LCD dimensions
void initRenderer(int width, int height) {
  lcdWidth = width;
  lcdHeight = height;
  
  // Initialize FPS tracking
  lastFrameTime = getMicros();
  for (int i = 0; i < FPS_SAMPLE_COUNT; i++) {
    frameTimes[i] = 1000000u / 60u; // Assume 60 FPS initially (~16667 µs)
  }
}

// Update FPS counter
void updateFPS() {
  uint32_t currentTime = getMicros();
  uint32_t delta = currentTime - lastFrameTime;
  lastFrameTime = currentTime;

  // Guard against zero delta (back-to-back calls within the same microsecond)
  if (delta == 0u) delta = 1u;

  // Store frame time (µs)
  frameTimes[frameIndex] = delta;
  frameIndex = (frameIndex + 1) % FPS_SAMPLE_COUNT;

  // Average frame time over the sample window
  uint32_t totalTime = 0u;
  for (int i = 0; i < FPS_SAMPLE_COUNT; i++) {
    totalTime += frameTimes[i];
  }

  // FPS = (samples × 1,000,000 µs/s) / total_µs
  currentFPS = (static_cast<float>(FPS_SAMPLE_COUNT) * 1000000.0f)
             / static_cast<float>(totalTime);
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
  if (fps > 99999) fps = 99999;
  
  // Extract digits
  int tenthousands = fps / 10000;
  int thousands    = (fps / 1000) % 10;
  int hundreds     = (fps / 100)  % 10;
  int tens         = (fps / 10)   % 10;
  int ones         =  fps         % 10;
  
  int x = FPS_DISPLAY_X;
  
  // Draw digits with spacing, suppressing leading zeros
  if (tenthousands > 0) {
    drawDigit(vram, x, FPS_DISPLAY_Y, tenthousands, COLOR_HIGHLIGHT);
    x += 6;
  }
  if (tenthousands > 0 || thousands > 0) {
    drawDigit(vram, x, FPS_DISPLAY_Y, thousands, COLOR_HIGHLIGHT);
    x += 6;
  }
  if (tenthousands > 0 || thousands > 0 || hundreds > 0) {
    drawDigit(vram, x, FPS_DISPLAY_Y, hundreds, COLOR_HIGHLIGHT);
    x += 6;
  }
  if (tenthousands > 0 || thousands > 0 || hundreds > 0 || tens > 0) {
    drawDigit(vram, x, FPS_DISPLAY_Y, tens, COLOR_HIGHLIGHT);
    x += 6;
  }
  drawDigit(vram, x, FPS_DISPLAY_Y, ones, COLOR_HIGHLIGHT);
}

// Draw the brush-size slider in the UI bar.
// Layout: digit showing current size | track | handle
static void drawBrushSlider(uint16_t* vram) {
  const int UI_Y = SCREEN_HEIGHT - UI_HEIGHT;

  // --- Digit: current brush size (1-9) ---
  drawDigit(vram,
            BRUSH_SLIDER_DIGIT_X,
            UI_Y + (UI_HEIGHT - 7) / 2,  // vertically centred in bar
            brushSize,
            COLOR_HIGHLIGHT);

  // --- Track: thin horizontal line ---
  const int trackY = UI_Y + UI_HEIGHT / 2;  // vertical centre of bar
  for (int tx = BRUSH_SLIDER_TRACK_X;
       tx < BRUSH_SLIDER_TRACK_X + BRUSH_SLIDER_TRACK_W;
       tx++) {
    if (tx >= 0 && tx < lcdWidth && trackY >= 0 && trackY < lcdHeight)
      vram[trackY * lcdWidth + tx] = COLOR_WALL;  // dark-grey track
  }

  // --- Handle: small filled rectangle ---
  // Position proportionally within the track
  const int effectiveW = BRUSH_SLIDER_TRACK_W - BRUSH_SLIDER_HANDLE_W;
  const int handleX = BRUSH_SLIDER_TRACK_X +
    (brushSize - BRUSH_SIZE_MIN) * effectiveW / (BRUSH_SIZE_MAX - BRUSH_SIZE_MIN);
  const int handleTop    = UI_Y + 2;
  const int handleBottom = UI_Y + UI_HEIGHT - 2;

  for (int hy = handleTop; hy < handleBottom; hy++) {
    for (int hx = handleX; hx < handleX + BRUSH_SLIDER_HANDLE_W; hx++) {
      if (hx >= 0 && hx < lcdWidth && hy >= 0 && hy < lcdHeight)
        vram[hy * lcdWidth + hx] = COLOR_HIGHLIGHT;
    }
  }
}

// Draw the grid to screen - optimized for faster VRAM writes
ILRAM_FUNC void drawGrid(uint16_t* vram) {
  // Optimized: write scanlines directly with proper bounds checking
  // Each grid cell is PIXEL_SIZE x PIXEL_SIZE pixels
  for (int y = 0; y < GRID_HEIGHT; y++) {
    int screenY = y * PIXEL_SIZE;
    uint16_t* scanline0 = vram + screenY * lcdWidth;
    uint16_t* scanline1 = vram + (screenY + 1) * lcdWidth;

    // Write the full first scanline before touching the second.
    // Sequential writes into one contiguous buffer are far more cache-friendly
    // than interleaving writes 768 bytes apart for every x column.
    for (int x = 0; x < GRID_WIDTH; x++) {
      uint16_t color = tempViewEnabled
        ? (grid[y][x] == Particle::WALL ? tempToColor(TEMP_AMBIENT) : tempToColor(tempGet(x, y)))
        : getParticleColorVaried(grid[y][x], x, y);
      int screenX = x * PIXEL_SIZE;
      scanline0[screenX]     = color;
      scanline0[screenX + 1] = color;
    }

    // scanline1 is pixel-perfect identical to scanline0 — bulk-copy it.
    // GRID_WIDTH * PIXEL_SIZE == SCREEN_WIDTH, so this covers the full row.
    memcpy(scanline1, scanline0, (size_t)GRID_WIDTH * PIXEL_SIZE * sizeof(uint16_t));
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

  // Draw brush size slider
  drawBrushSlider(vram);
}
