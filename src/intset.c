#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <stdint.h>

#include "intset.h"
#include "zmalloc.h"
#include "endianconv.h"

/*
 * Note that these encoding are ordered, so
 * INTSET_ENC_INT16 < INTSET_ENC_INT32 < INTSET_ENC_INT64.
*/
/*
 * intset 的编码方式
*/
#define INTSET_ENC_INT16 (sizeof(int16_t))
#define INTSET_ENC_INT32 (sizeof(int32_t))
#define INTSET_ENC_INT64 (sizeof(int64_t))

/* Return the required encoding for the provided value */
/*
 * 返回适用于传入值 v 的编码方式
 * 
 * T = O(1)
*/
static uint8_t _intsetValueEncoding(int64_t v) {
    if (v < INT32_MIN || v > INT32_MAX) 
        return INTSET_ENC_INT64;
    else if (v < INT16_MIN || v > INT16_MAX)
        return INTSET_ENC_INT32;
    else 
        return INTSET_ENC_INT16;
}