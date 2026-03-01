#include "random.h"

// Single definition of the shared PRNG state.
// All translation units that include random.h use this one instance.
uint32_t xorshift_state = 0x12345678;
