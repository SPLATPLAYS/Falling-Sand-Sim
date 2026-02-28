#ifndef PARTICLE_H
#define PARTICLE_H

#include "config.h"

// Get color for particle type
inline uint16_t getParticleColor(Particle p) {
  switch (p) {
    case Particle::SAND: return COLOR_SAND;
    case Particle::WATER: return COLOR_WATER;
    case Particle::STONE: return COLOR_STONE;
    case Particle::WALL: return COLOR_WALL;
    case Particle::LAVA: return COLOR_LAVA;
    case Particle::PLANT: return COLOR_PLANT;
    default: return COLOR_AIR;
  }
}

// Get fall speed for a particle type (lower = faster)
inline int getFallSpeed(Particle p) {
  switch (p) {
    case Particle::STONE: return FALL_SPEED_STONE;
    case Particle::SAND: return FALL_SPEED_SAND;
    case Particle::WATER: return FALL_SPEED_WATER;
    case Particle::LAVA: return FALL_SPEED_LAVA;
    default: return 1; // Stationary particles, doesn't matter
  }
}

// Get default temperature for a particle type
inline uint8_t getParticleTemperature(Particle p) {
  switch (p) {
    case Particle::LAVA: return TEMP_LAVA;      // Very hot
    case Particle::WATER: return TEMP_COLD;     // Cool
    case Particle::SAND: return TEMP_AMBIENT;
    case Particle::STONE: return TEMP_AMBIENT;
    case Particle::WALL: return TEMP_AMBIENT;
    case Particle::PLANT: return TEMP_AMBIENT;
    case Particle::AIR: return TEMP_AMBIENT;
    default: return TEMP_AMBIENT;
  }
}

// Get color for a particle with a small coordinate-derived variation.
// Perturbs the two LSBs of the green channel (bits 5-6 in RGB565) using a
// cheap per-cell hash so that flat fills of sand/water/etc. look "grainy"
// rather than solid blocks — zero extra memory required.
// AIR and WALL are left unvaried so backgrounds and structures stay clean.
inline uint16_t getParticleColorVaried(Particle p, int x, int y) {
  uint16_t base = getParticleColor(p);
  if (p == Particle::AIR || p == Particle::WALL) return base;
  // Stable, cheap hash: different primes on each axis prevent axis-aligned banding
  uint8_t v = static_cast<uint8_t>(static_cast<uint8_t>(x * 3u)
                                  ^ static_cast<uint8_t>(y * 7u));
  // XOR bits 5-6 (green LSBs): ±0..3 steps on green, hue is preserved
  return base ^ (static_cast<uint16_t>(v & 0x3u) << 5);
}

// Map temperature (0-255) to an RGB565 heat-map colour.
// Uses a 32-entry ROM palette (64 bytes in flash) — zero extra RAM cost.
// deep-blue (cold=0) → black (ambient≈50) → red → orange → yellow → white (lava=255)
inline uint16_t tempToColor(uint8_t t) {
  static const uint16_t palette[32] = {
    0x001E, 0x001C, 0x0018, 0x0012, 0x000C, 0x0006, 0x0002, 0x0000, // 0..55   cold (blue→black)
    0x1000, 0x2800, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xE000, // 56..127  warming (black→red)
    0xF880, 0xF940, 0xFA40, 0xFB40, 0xFC40, 0xFD40, 0xFE40, 0xFF40, // 128..199 hot (red→yellow)
    0xFFE0, 0xFFE4, 0xFFE8, 0xFFEE, 0xFFF4, 0xFFFA, 0xFFED, 0xFFFF  // 200..255 lava (yellow→white)
  };
  return palette[t >> 3];
}

#endif // PARTICLE_H
