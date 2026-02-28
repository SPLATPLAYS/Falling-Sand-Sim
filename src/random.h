#ifndef RANDOM_H
#define RANDOM_H

#include <cstdint>

// XorShift32 PRNG - fast and lightweight for embedded systems
static uint32_t xorshift_state = 0x12345678;

inline uint32_t xorshift32() {
  uint32_t x = xorshift_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  xorshift_state = x;
  return x;
}

#endif // RANDOM_H
