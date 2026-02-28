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
  PLANT
};

// Color definitions (RGB565 format)
constexpr uint16_t COLOR_AIR = 0x0000;     // Black
constexpr uint16_t COLOR_SAND = 0xFDA0;    // Sandy yellow
constexpr uint16_t COLOR_WATER = 0x03BF;   // Blue
constexpr uint16_t COLOR_STONE = 0x7BEF;   // Gray
constexpr uint16_t COLOR_WALL = 0x4208;    // Dark gray
constexpr uint16_t COLOR_LAVA = 0xF800;    // Bright red-orange
constexpr uint16_t COLOR_PLANT = 0x07E0;   // Green
constexpr uint16_t COLOR_UI_AIR = 0xF81F;  // Bright pink (magenta) for UI display
constexpr uint16_t COLOR_HIGHLIGHT = 0xFFFF; // White

// Brush size
constexpr int BRUSH_SIZE = 3;

// UI constants
constexpr int UI_HEIGHT = 16;
constexpr int SWATCH_SIZE = 16;
constexpr int SWATCH_SPACING = 20;
constexpr int UI_START_X = 10;
constexpr int PARTICLE_TYPE_COUNT = 7;

// Frame skipping for performance
constexpr int FRAME_SKIP_AMOUNT = 0;       // 0 = no skip, 1 = skip 1 frame (30fps), 2 = skip 2 frames (20fps), etc.
constexpr bool FRAME_SKIP_ENABLED = false; // Set to true to enable frame skipping

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
// 1 = updates every frame, 2 = updates 50% of frames, 3 = updates 33% of frames, etc.
constexpr int FALL_SPEED_STONE = 1;  // Heavy - falls fastest
constexpr int FALL_SPEED_SAND = 2;   // Medium - normal fall speed
constexpr int FALL_SPEED_WATER = 1;  // Liquid - flows fast
constexpr int FALL_SPEED_LAVA = 2;   // Heavy liquid - flows slower than water

// Temperature constants (0-255 scale)
constexpr uint8_t TEMP_AMBIENT = 50;      // Default/room temperature
constexpr uint8_t TEMP_COLD = 20;         // Cold temperature
constexpr uint8_t TEMP_HOT = 200;         // Hot temperature
constexpr uint8_t TEMP_LAVA = 255;        // Maximum temperature (lava)

#endif // CONFIG_H
