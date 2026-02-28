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

#endif // PARTICLE_H
