#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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