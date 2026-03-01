#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

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
  WALL,
  LAVA,
  PLANT,
  ICE
};

// Color definitions (RGB565 format)
constexpr uint16_t COLOR_AIR = 0x0000;     // Black
constexpr uint16_t COLOR_SAND = 0xFDA0;    // Sandy yellow
constexpr uint16_t COLOR_WATER = 0x03BF;   // Blue
constexpr uint16_t COLOR_STONE = 0x7BEF;   // Gray
constexpr uint16_t COLOR_WALL = 0x4208;    // Dark gray
constexpr uint16_t COLOR_LAVA = 0xF800;    // Bright red-orange
constexpr uint16_t COLOR_PLANT = 0x07E0;   // Green
constexpr uint16_t COLOR_ICE   = 0xAFFF;   // Pale icy cyan-blue
constexpr uint16_t COLOR_UI_AIR = 0xF81F;  // Bright pink (magenta) for UI display
constexpr uint16_t COLOR_HIGHLIGHT = 0xFFFF; // White

// Brush size (runtime variable, persisted via MCS)
constexpr int BRUSH_SIZE_DEFAULT = 3;
constexpr int BRUSH_SIZE_MIN     = 1;
constexpr int BRUSH_SIZE_MAX     = 9;

// UI constants
constexpr int UI_HEIGHT = 16;
constexpr int SWATCH_SIZE = 16;
constexpr int SWATCH_SPACING = 20;
constexpr int UI_START_X = 10;
constexpr int PARTICLE_TYPE_COUNT = 8;

// Brush size slider layout (placed after particle swatches in the UI bar)
// Swatches occupy x = 10 to 10 + 8*20 = 170; slider starts at 172
constexpr int BRUSH_SLIDER_DIGIT_X  = 172; // X of brush-size digit
constexpr int BRUSH_SLIDER_TRACK_X  = 182; // X where slider track begins
constexpr int BRUSH_SLIDER_TRACK_W  = 80;  // Pixel width of track
constexpr int BRUSH_SLIDER_HANDLE_W = 6;   // Pixel width of handle

// Simulation speed mode — runtime variable persisted via MCS (see settings.h).
// Mode 0 = full render rate (no skip); modes 1-4 = progressively fewer renders.
// Skip amounts per mode: {0, 1, 2, 4, 8} frames skipped between each render.
constexpr int SIM_SPEED_MODE_DEFAULT = 0;
constexpr int SIM_SPEED_MODE_MAX     = 4;
// Names shown in the settings screen
extern const char* const simSpeedModeNames[SIM_SPEED_MODE_MAX + 1];
// Map mode → number of frames skipped between each render (0 = no skip)
extern const int simSkipAmounts[SIM_SPEED_MODE_MAX + 1];

// Simulation probabilities and limits
constexpr int LAVA_FLOW_CHANCE = 3;        // 1 in 3 chance to flow sideways
constexpr int PLANT_GROWTH_CHANCE = 10;    // 1 in 10 chance to grow per frame
constexpr int PLANT_GROWTH_ATTEMPTS = 4;   // Max attempts to find empty cell for growth

// FPS counter constants
constexpr int FPS_SAMPLE_COUNT = 30;       // Average FPS over last 30 frames
constexpr int FPS_DISPLAY_X = 300;         // X position for FPS display
constexpr int FPS_DISPLAY_Y = 2;           // Y position for FPS display

// Note: When frame skipping is enabled, the displayed FPS represents rendered frames per second,
// not total simulation updates. Physics continues to update at full speed.

// Particle fall speeds (lower = faster, represents update frequency)
// 1 = updates every frame, 2 = updates 50% of frames, 4 = updates 25% of frames, etc.
// MUST be powers of 2: shouldUpdate() uses (xorshift32() & (speed-1)) instead of
// modulo to avoid SH4's slow software-emulated integer division.
constexpr int FALL_SPEED_STONE = 1;  // Heavy - falls fastest
constexpr int FALL_SPEED_SAND = 2;   // Medium - normal fall speed
constexpr int FALL_SPEED_WATER = 1;  // Liquid - flows fast
constexpr int FALL_SPEED_LAVA = 2;   // Heavy liquid - flows slower than water
constexpr int FALL_SPEED_ICE  = 2;   // Solid - falls like sand

// Enforce power-of-2 constraint at compile time
static_assert((FALL_SPEED_STONE & (FALL_SPEED_STONE - 1)) == 0, "FALL_SPEED_STONE must be a power of 2");
static_assert((FALL_SPEED_SAND  & (FALL_SPEED_SAND  - 1)) == 0, "FALL_SPEED_SAND must be a power of 2");
static_assert((FALL_SPEED_WATER & (FALL_SPEED_WATER - 1)) == 0, "FALL_SPEED_WATER must be a power of 2");
static_assert((FALL_SPEED_LAVA  & (FALL_SPEED_LAVA  - 1)) == 0, "FALL_SPEED_LAVA must be a power of 2");
static_assert((FALL_SPEED_ICE   & (FALL_SPEED_ICE   - 1)) == 0, "FALL_SPEED_ICE must be a power of 2");

// Coarse temperature grid (¼ resolution: 1 cell covers 4×4 fine cells)
constexpr int TEMP_SCALE   = 4;
constexpr int TEMP_GRID_W  = GRID_WIDTH  / TEMP_SCALE;  // 48
constexpr int TEMP_GRID_H  = GRID_HEIGHT / TEMP_SCALE;  // 24

// Grid row split across on-chip X/Y RAM (4 KB per bank, 2 banks each = 8 KB each)
// X RAM holds rows 0..41  (42 × 192 = 8,064 bytes < 8,192)
// Y RAM holds rows 42..83 (42 × 192 = 8,064 bytes < 8,192)
// Regular RAM holds rows 84..95 (12 × 192 = 2,304 bytes)
constexpr int GRID_ROWS_X    = 42;
constexpr int GRID_ROWS_Y    = 42;
constexpr int GRID_ROWS_REST = GRID_HEIGHT - GRID_ROWS_X - GRID_ROWS_Y; // 12

// ILRAM placement: functions marked with this attribute are placed in the
// SH7305's internal instruction RAM, which is significantly faster to fetch
// and execute than regular external flash/SDRAM.
#define ILRAM_FUNC __attribute__((section(".ilram")))

// Temperature constants (0-255 scale)
constexpr uint8_t TEMP_AMBIENT = 50;      // Default/room temperature
constexpr uint8_t TEMP_COLD = 20;         // Cold temperature
constexpr uint8_t TEMP_HOT = 200;         // Hot temperature
constexpr uint8_t TEMP_LAVA = 255;        // Maximum temperature (lava)
constexpr uint8_t TEMP_ICE_SURFACE = 5;   // Temperature ICE pins its coarse tile to
constexpr uint8_t TEMP_FREEZE_WATER = 12; // Water freezes to ICE at or below this temp
constexpr uint8_t TEMP_ICE_MELT = 65;     // ICE melts to water at or above this temp

// Coarse-tile cooling rates (used in propagateTemperature Step 2)
// Water-occupied tiles cool by TEMP_WATER_COOL_RATE per tick toward TEMP_COLD.
constexpr int TEMP_WATER_COOL_RATE   = 3;
// Air-exposed tiles (any AIR cell in the coarse tile) drift toward TEMP_AMBIENT
// by 1 every tick.  Buried tiles (no AIR) drift at 1-in-16 rate.
constexpr uint32_t TEMP_BURIED_COOL_MASK = 0xFu;  // 1 in 16 chance

// Lava <-> stone conversion temperature thresholds
// Lava that is isolated (no adjacent lava) and whose coarse tile drops below
// TEMP_LAVA_SOLIDIFY will slowly solidify into stone.
constexpr uint8_t TEMP_LAVA_SOLIDIFY = 110;
// Stone whose coarse tile exceeds TEMP_STONE_MELT (requires being surrounded
// by multiple lava cells for the diffused temperature to reach this level)
// will slowly melt back into lava.
constexpr uint8_t TEMP_STONE_MELT    = 230;

// Number of diffusion passes per physics tick.
// Each pass spreads heat one coarse cell further (one coarse cell = 4 fine cells).
// Higher = faster, more visible spread; lower = cheaper.
constexpr int TEMP_DIFFUSION_PASSES = 4;

// First coarse row that lies entirely within the UI bar.
// Coarse tiles at cy >= this value are always pinned to TEMP_AMBIENT so heat
// cannot bleed behind the particle-selector UI at the bottom of the screen.
constexpr int TEMP_UI_COARSE_ROW = (SCREEN_HEIGHT - UI_HEIGHT) / PIXEL_SIZE / TEMP_SCALE;

#endif // CONFIG_H
