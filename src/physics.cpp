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
  // Blend each cell with its 4 neighbours.  To avoid a variable divisor
  // (expensive on SH4, which has no hardware divide), weight = self×4 + up to
  // 4 neighbours, then >> 3 (divide by 8).  Missing edge neighbours count 0.
  for (int cy = 0; cy < TEMP_GRID_H; cy++) {
    for (int cx = 0; cx < TEMP_GRID_W; cx++) {
      int t = temperature[cy][cx];
      int sum = t << 2; // self * 4
      if (cx > 0)               sum += temperature[cy][cx - 1];
      if (cx < TEMP_GRID_W - 1) sum += temperature[cy][cx + 1];
      if (cy > 0)               sum += temperature[cy - 1][cx];
      if (cy < TEMP_GRID_H - 1) sum += temperature[cy + 1][cx];
      int avg = sum >> 3; // divide by 8
      // Drift one step towards ambient temperature
      if      (avg > TEMP_AMBIENT) avg--;
      else if (avg < TEMP_AMBIENT) avg++;
      temperature[cy][cx] = static_cast<uint8_t>(avg);
    }
  }

  // --- Step 2: Inject sources / sinks from actual particles ---
  // Lava pins its coarse tile to TEMP_LAVA; water gradually cools its tile.
  for (int cy = 0; cy < TEMP_GRID_H; cy++) {
    for (int cx = 0; cx < TEMP_GRID_W; cx++) {
      const int fineX0 = cx * TEMP_SCALE;
      const int fineY0 = cy * TEMP_SCALE;
      bool hasLava   = false;
      bool hasWater  = false;
      bool hasWall   = false; // any WALL cell → thermal barrier
      for (int dy = 0; dy < TEMP_SCALE; dy++) {
        for (int dx = 0; dx < TEMP_SCALE; dx++) {
          Particle p = grid[fineY0 + dy][fineX0 + dx];
          if (p == Particle::LAVA)  { hasLava = true; break; }
          if (p == Particle::WALL)    hasWall  = true;
          if (p == Particle::WATER)   hasWater = true;
        }
        if (hasLava) break;
      }
      if (hasLava) {
        temperature[cy][cx] = TEMP_LAVA;
      } else if (hasWall) {
        // Coarse cell contains a wall — act as thermal insulator so heat
        // cannot bleed through boundary/player walls or into the UI zone.
        temperature[cy][cx] = TEMP_AMBIENT;
      } else if (hasWater && temperature[cy][cx] > TEMP_COLD) {
        temperature[cy][cx]--; // water slowly cools its coarse tile
      }
      // Pure-air cells: no special override — diffusion in Step 1 already
      // drifts them toward ambient gradually.
    }
  }
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
  if (isEmpty(x, y + 1)) {
    swap(x, y, x, y + 1);
    updatedSet(x, y);
    updatedSet(x, y + 1);
  }
}
static void updateLava(int x, int y) {
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
