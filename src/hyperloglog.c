#include "redis.h"

#include <stdint.h>
#include <math.h>

struct hlldhr {
    char magic[4];          // "HYLL"
    uint8_t encoding;       // HLL_DENSE or HLL_SPARSE
    uint8_t notused[3];     // Reversed for future use, must be zero
    uint8_t card[8];        // Cached cardinality, little endian
    uint8_t registers[];    // Data bytes
};

/*
 * The cached cardinality MSB is used to signal validity of the cached value
*/
#define HLL_INVALIDATE_CACHE(hdr) (hdr)->card[0] |= (1<<7)
#define HLL_VALID_CACHE(hdr) (((hdr)->card[0] & (1<<7)) == 0)

#define HLL_P 14                // The greater is P, the smaller the error
#define HLL_REGISTERS (1 << HLL_P)  // With p=14, 16384 registers
#define HLL_P_MASK (HLL_REGISTERS - 1)  // Mask to index register
#define HLL_BITS 6              // Enough to count up to 63 leading zeroes
#define HLL_REGISTER_MAX ((1 << HLL_BITS) - 1)
#define HLL_HDR_SIZE sizeof(struct hllhdr)
#define HLL_DENSE_SIZE (HLL_HDR_SIZE + ((HLL_REGISTERS * HLL_BITS + 7)/ 8))
#define HLL_DENSE 0             // Dense encoding
#define HLL_SPARSE 1            // Sparse encoding
#define HLL_RAW 255             // Only used internally, never exposed
#define HLL_MAX_ENCODING 1

static char *invalid_hll_err = "-INVALIDOBJ Corrupted HLL object detected\r\n";

/* =========================== Low level bit macros ========================= */
