#include "redis.h"

#include <stdint.h>
#include <math.h>

struct hlldhr {
    char magic[4];          // "HYLL"
    uint8_t encoding;       // HLL_DENSE or HLL_SPARSE
    uint8_t notused[3];     // Reversed for future use, must be zero
    uint8_t card[8];        // Cached cardinality, little endian
    uint8_t registers[];    // Data bytes
}