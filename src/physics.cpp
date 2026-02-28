#include "physics.h"
#include "grid.h"
#include "particle.h"
#include "random.h"
#include <cstring>

// Check if a particle should update this frame based on its fall speed.
// Fall speeds MUST be powers of 2 (enforced by static_assert in config.h) so
// that the cheap bitwise AND replaces the integer division that `%` compiles to
// on SH4 (which has no hardware divide instruction — it is a software call).
static bool shouldUpdate(Particle p) {
  int fallSpeed = getFallSpeed(p);
  if (fallSpeed <= 1) return true;  // skip PRNG call for always-update particles
  return (xorshift32() & (fallSpeed - 1)) == 0;
}

// Propagate temperature: diffuse heat between coarse cells then re-inject
// particle-sourced heat/cold.  The coarse grid is only 24×48 (1,152 cells)
// so running every physics tick is negligible cost.
static void propagateTemperature() {
  // --- Step 1: Diffusion + ambient cooling ---
  // Run TEMP_DIFFUSION_PASSES passes so heat spreads TEMP_DIFFUSION_PASSES
  // coarse cells per tick — visibly flowing away from lava into neighbours.
  // Each pass: blend with 4-neighbour average using power-of-2 divisor so
  // the SH4 (no hardware divide) can use a cheap right-shift instead.
  // Weights: self×4 + each present neighbour×1, then >>3 (÷8).
  // Ambient drift runs only on the final pass to avoid over-cooling during
  // the intermediate passes.
  // Pure diffusion — no ambient drift here; cooling is applied per‑tile
  // in Step 2 where we know the context (air / water / buried).
  for (int pass = 0; pass < TEMP_DIFFUSION_PASSES; pass++) {
    for (int cy = 0; cy < TEMP_GRID_H; cy++) {
      for (int cx = 0; cx < TEMP_GRID_W; cx++) {
        int t = temperature[cy][cx];
        // Clamp missing edge neighbours to the cell's own value so absent
        // borders don't artificially cool/heat edge cells.
        int tL = (cx > 0)               ? (int)temperature[cy][cx - 1] : t;
        int tR = (cx < TEMP_GRID_W - 1) ? (int)temperature[cy][cx + 1] : t;
        int tU = (cy > 0)               ? (int)temperature[cy - 1][cx] : t;
        int tD = (cy < TEMP_GRID_H - 1) ? (int)temperature[cy + 1][cx] : t;
        // Always exactly 8 contributions → safe power-of-2 shift
        temperature[cy][cx] = static_cast<uint8_t>(((t << 2) + tL + tR + tU + tD) >> 3);
      }
    }
  }

  // --- Step 2: Inject sources / sinks from actual particles ---
  // Only process coarse rows above the UI zone; rows at or below
  // TEMP_UI_COARSE_ROW are always pinned to TEMP_AMBIENT (cleared below).
  for (int cy = 0; cy < TEMP_UI_COARSE_ROW; cy++) {
    for (int cx = 0; cx < TEMP_GRID_W; cx++) {
      const int fineX0 = cx * TEMP_SCALE;
      const int fineY0 = cy * TEMP_SCALE;
      bool hasLava  = false;
      bool hasWater = false;
      int  wallCount = 0;
      int  airCount  = 0;
      for (int dy = 0; dy < TEMP_SCALE; dy++) {
        for (int dx = 0; dx < TEMP_SCALE; dx++) {
          Particle p = grid[fineY0 + dy][fineX0 + dx];
          if      (p == Particle::LAVA)  { hasLava = true; }
          else if (p == Particle::WALL)  { wallCount++; }
          else if (p == Particle::WATER) { hasWater = true; }
          else if (p == Particle::AIR)   { airCount++; }
        }
      }
      const bool allWall = (wallCount == TEMP_SCALE * TEMP_SCALE);
      if (hasLava) {
        // Lava pins its tile to maximum heat — it is a continuous heat source.
        temperature[cy][cx] = TEMP_LAVA;
      } else if (allWall) {
        // Entirely-wall tile: thermal insulator, always ambient.
        temperature[cy][cx] = TEMP_AMBIENT;
      } else {
        // Context-aware cooling:
        //  • Water present  → aggressively cool toward TEMP_COLD
        //  • Air present    → slow drift toward TEMP_AMBIENT (1/tick)
        //  • Buried (solid) → negligible cooling (1-in-16 chance)
        int t = static_cast<int>(temperature[cy][cx]);
        if (hasWater) {
          t -= TEMP_WATER_COOL_RATE;
          if (t < TEMP_COLD) t = TEMP_COLD;
        } else if (airCount > 0) {
          if      (t > TEMP_AMBIENT) t--;
          else if (t < TEMP_AMBIENT) t++;
        } else {
          // Buried — very slowly return to ambient
          if (t != TEMP_AMBIENT && (xorshift32() & TEMP_BURIED_COOL_MASK) == 0) {
            if (t > TEMP_AMBIENT) t--;
            else                  t++;
          }
        }
        temperature[cy][cx] = static_cast<uint8_t>(t);
      }
      // (no separate comment needed — cooling handled above)
    }
  }

  // Pin UI-zone coarse rows to TEMP_AMBIENT so heat never bleeds behind
  // the particle-selector bar.  memset is used here rather than a loop so
  // the compiler can trivially verify the write is within bounds.
  memset(&temperature[TEMP_UI_COARSE_ROW][0], TEMP_AMBIENT,
         (TEMP_GRID_H - TEMP_UI_COARSE_ROW) * TEMP_GRID_W);
}

// Update sand particle
static void updateSand(int x, int y) {
  // Temperature: sustained heat (from nearby lava) converts sand to stone
  if (tempGet(x, y) >= TEMP_HOT && (xorshift32() & 0xFu) == 0) {
    grid[y][x] = Particle::STONE;
    tempSet(x, y, TEMP_AMBIENT);
    return;
  }

  // Try to fall straight down
  if (canMoveTo(x, y + 1, Particle::SAND)) {
    swap(x, y, x, y + 1);
    updatedSet(x, y);
    updatedSet(x, y + 1);
  }
  // Try diagonal down-left
  else if (canMoveTo(x - 1, y + 1, Particle::SAND)) {
    swap(x, y, x - 1, y + 1);
    updatedSet(x, y);
    updatedSet(x - 1, y + 1);
  }
  // Try diagonal down-right
  else if (canMoveTo(x + 1, y + 1, Particle::SAND)) {
    swap(x, y, x + 1, y + 1);
    updatedSet(x, y);
    updatedSet(x + 1, y + 1);
  }
}

// Update water particle
static void updateWater(int x, int y) {
  // Temperature: high heat evaporates water (range effect via coarse grid)
  if (tempGet(x, y) >= TEMP_HOT && (xorshift32() & 0x7u) == 0) {
    grid[y][x] = Particle::AIR;
    tempSet(x, y, TEMP_AMBIENT);
    return;
  }

  // Try to fall straight down (only into empty space)
  if (isEmpty(x, y + 1)) {
    swap(x, y, x, y + 1);
    updatedSet(x, y);
    updatedSet(x, y + 1);
  }
  // Try diagonal down-left (only into empty space)
  else if (isEmpty(x - 1, y + 1)) {
    swap(x, y, x - 1, y + 1);
    updatedSet(x, y);
    updatedSet(x - 1, y + 1);
  }
  // Try diagonal down-right (only into empty space)
  else if (isEmpty(x + 1, y + 1)) {
    swap(x, y, x + 1, y + 1);
    updatedSet(x, y);
    updatedSet(x + 1, y + 1);
  }
  // Try to flow sideways - randomize direction for balanced spreading
  else {
    bool tryLeftFirst = (xorshift32() & 1) == 0;
    int dir1 = tryLeftFirst ? -1 : 1;
    int dir2 = tryLeftFirst ? 1 : -1;

    if (isEmpty(x + dir1, y)) {
      swap(x, y, x + dir1, y);
      updatedSet(x, y);
      updatedSet(x + dir1, y);
    }
    else if (isEmpty(x + dir2, y)) {
      swap(x, y, x + dir2, y);
      updatedSet(x, y);
      updatedSet(x + dir2, y);
    }
  }
}

// Update stone particle (just falls, no sideways movement)
static void updateStone(int x, int y) {
  // Stone submerged in extreme heat (needs multiple nearby lava cells to
  // push the coarse tile past TEMP_STONE_MELT) slowly melts back to lava.
  if (tempGet(x, y) >= TEMP_STONE_MELT && (xorshift32() & 0x1Fu) == 0) {
    grid[y][x] = Particle::LAVA;
    tempSet(x, y, TEMP_LAVA);
    return;
  }

  if (isEmpty(x, y + 1)) {
    swap(x, y, x, y + 1);
    updatedSet(x, y);
    updatedSet(x, y + 1);
  }
}
static void updateLava(int x, int y) {
  // Isolated lava (no adjacent lava cell) slowly solidifies into stone,
  // modelling a thin tendril of lava losing heat to its surroundings.
  // Lava inside a larger pool (has neighbours) stays molten indefinitely.
  bool hasAdjacentLava = false;
  for (int dy = -1; dy <= 1 && !hasAdjacentLava; dy++) {
    for (int dx = -1; dx <= 1 && !hasAdjacentLava; dx++) {
      if (dx == 0 && dy == 0) continue;
      if (isValid(x + dx, y + dy) && grid[y + dy][x + dx] == Particle::LAVA)
        hasAdjacentLava = true;
    }
  }
  // Low probability so solidification takes many seconds, not instant.
  // Also require the coarse tile has cooled somewhat (water quenching is
  // the main fast-solidification path).
  if (!hasAdjacentLava && tempGet(x, y) < TEMP_LAVA &&
      (xorshift32() & 0xFFu) == 0) {
    grid[y][x] = Particle::STONE;
    return;
  }

  // Check and convert adjacent particles
  // Check all 8 neighbors for sand/water/plant
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = x + dx;
      int ny = y + dy;
      if (isValid(nx, ny)) {
        if (grid[ny][nx] == Particle::SAND) {
          grid[ny][nx] = Particle::STONE;
        } else if (grid[ny][nx] == Particle::WATER) {
          grid[ny][nx] = Particle::AIR;
        } else if (grid[ny][nx] == Particle::PLANT) {
          grid[ny][nx] = Particle::AIR;  // Burn plant
        }
      }
    }
  }
  
  // Lava flows like water but slower
  if (isEmpty(x, y + 1)) {
    swap(x, y, x, y + 1);
    updatedSet(x, y);
    updatedSet(x, y + 1);
  }
  // Try diagonal down-left
  else if (isEmpty(x - 1, y + 1)) {
    swap(x, y, x - 1, y + 1);
    updatedSet(x, y);
    updatedSet(x - 1, y + 1);
  }
  // Try diagonal down-right
  else if (isEmpty(x + 1, y + 1)) {
    swap(x, y, x + 1, y + 1);
    updatedSet(x, y);
    updatedSet(x + 1, y + 1);
  }
  // Occasionally flow sideways
  else if (xorshift32() % LAVA_FLOW_CHANCE == 0) {
    bool tryLeftFirst = (xorshift32() & 1) == 0;
    int dir = tryLeftFirst ? -1 : 1;
    if (isEmpty(x + dir, y)) {
      swap(x, y, x + dir, y);
      updatedSet(x, y);
      updatedSet(x + dir, y);
    }
  }
}

// Update plant particle
static void updatePlant(int x, int y) {
  // Temperature: sustained heat burns plant (range effect via coarse grid)
  if (tempGet(x, y) >= TEMP_HOT && (xorshift32() & 0x3u) == 0) {
    grid[y][x] = Particle::AIR;
    return;
  }

  // Check for lava in adjacent cells - plant burns
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = x + dx;
      int ny = y + dy;
      if (isValid(nx, ny) && grid[ny][nx] == Particle::LAVA) {
        grid[y][x] = Particle::AIR;  // Burn plant
        return;
      }
    }
  }
  
  // Check for water in adjacent cells - plant grows
  bool hasWater = false;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dx == 0 && dy == 0) continue;
      int nx = x + dx;
      int ny = y + dy;
      if (isValid(nx, ny) && grid[ny][nx] == Particle::WATER) {
        hasWater = true;
        break;
      }
    }
    if (hasWater) break;
  }
  
  // If touching water, occasionally grow into adjacent empty spaces
  if (hasWater && xorshift32() % PLANT_GROWTH_CHANCE == 0) {
    // Try to grow in a random adjacent empty cell
    int attempts = 0;
    while (attempts < PLANT_GROWTH_ATTEMPTS) {
      int dx = (xorshift32() % 3) - 1;  // -1, 0, or 1
      int dy = (xorshift32() % 3) - 1;
      if (dx == 0 && dy == 0) {
        attempts++;
        continue;
      }
      int nx = x + dx;
      int ny = y + dy;
      if (isEmpty(nx, ny)) {
        grid[ny][nx] = Particle::PLANT;
        break;
      }
      attempts++;
    }
  }
}

// Simulate one step
ILRAM_FUNC void simulate() {
  // Clear update flags
  memset(updated, 0, sizeof(updated));

  // Propagate temperature (coarse 24×48 grid — cheap every frame)
  propagateTemperature();

  // Update from bottom to top, randomizing left-right order
  for (int y = GRID_HEIGHT - 2; y >= 0; y--) {
    // Alternate scan direction for more natural behavior
    bool scanLeft = (y % 2) == 0;
    
    for (int i = 0; i < GRID_WIDTH; i++) {
      int x = scanLeft ? i : (GRID_WIDTH - 1 - i);
      
      // Skip if already updated this frame
      if (updatedGet(x, y)) continue;
      
      Particle p = grid[y][x];

      // AIR and WALL never move — skip before any further work
      // (bitset read, PRNG call, switch) to avoid wasting cycles on the
      // majority of cells which are typically empty or static.
      if (p == Particle::AIR || p == Particle::WALL) continue;

      // Check if particle should update based on its density/fall speed
      if (!shouldUpdate(p)) continue;
      
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
        case Particle::LAVA:
          updateLava(x, y);
          break;
        case Particle::PLANT:
          updatePlant(x, y);
          break;
        default:
          break;
      }
    }
  }
}
