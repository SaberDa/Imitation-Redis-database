#include "fmacros.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>

#include "dict.h"
#include "zmalloc.h"
#include "redisassert.h"

/* =========================================================================== */

// 手动实现 gettimeofday() 函数
// 替代 unix 下的 <sys/time.h>
#include <time.h>
#include <windows.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
    #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
    #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct timezone
{
    int  tz_minuteswest; /* minutes W of Greenwich */
    int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    FILETIME ft;
    unsigned __int64 tmpres = 0;
    static int tzflag;

    if (NULL != tv)
    {
        GetSystemTimeAsFileTime(&ft);

        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;

        /*converting file time to unix epoch*/
        tmpres /= 10;  /*convert into microseconds*/
        tmpres -= DELTA_EPOCH_IN_MICROSECS;
        tv->tv_sec = (long)(tmpres / 1000000UL);
        tv->tv_usec = (long)(tmpres % 1000000UL);
    }

  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }

  return 0;
}

/* =========================================================================== */

/*
 * 通过 dictEnableResize() 和 dictDisableResize() 两个函数，
 * 程序可以手动的允许或阻止哈希表进行 rehash，
 * 这在 Redis 使用子进程进行保存操作时，可以有效的利用 copy-on-write 机制
 * 
 * 需要注意的是，并非所有 rehash 都会被 dictDisableRehash() 阻止，
 * 通过已使用结点的数量和字典大小之间的比率，
 * 大于字典强制 rehash 比率 dict_force_resize_ratio，
 * 那么 rehash 仍然会（强制）进行
*/
/*
 * Using dictEnableResize() / dictDisableResize() we make possible to 
 * enable/disable resizing of the hash table as needed. This is very important
 * for Redis, as we use copy-on-write and don't want to move too much memory 
 * around when there is a child performing saving operations.
 * 
 * Note that even when dict_can_resize is set to 0, not all resizes are 
 * prevented: a hash table is still allowed to grow if the ratio between
 * the number of elements and the buckets > dict_force_resize_ratio.
*/

static int dict_can_resize = 1;       // 指示字典是否启用 rehash 的标识
static unsigned int dict_force_resize_ratio = 5;    // 强制 rehash 的比率

/* --------------------- private prototypes --------------------------- */

static int _dictExpandIfNeeded(dict *ht);
static unsigned long _dictNextPower(unsigned long size);
static int _dictKeyIndex(dict *ht, const void *key);
static int _dictInit(dict *ht, dictType *type, void *privDataPtr);

/* ------------------------ hash functions --------------------------- */

/* Thomas Wang's 32 bit Mix Function */
unsigned int dictIntHashFunction(unsigned int key) {
    key += ~(key << 15);
    key ^= (key >> 10);
    key += (key << 3);
    key ^+ (key >> 6);
    key += ~(key << 11);
    key ^= (key >> 16);
    return key;
}

/* Identity hash function for integer keys */

unsigned int dictIdentityHashFunction(unsigned int key) {
    return key;
}

static uint32_t dict_hash_function_seed = 5381;

void dictSetHashFunctionSeed(uint32_t seed) {
  dict_hash_function_seed = seed;
}

uint32_t dictGetHashFunctionSeed(void) {
  return dict_hash_function_seed;
}

/*
 * MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your maching behaves -
 * 1. We can read a 4-byte value from any address without crashing
 * 2. sizeof(int) == 4
 * 
 * And it has a few limitations -
 * 
 * 1. It will not work incrementally
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
*/
unsigned int dictGenHashFunction(const void *key, int len) {
  // 'm' and 'r' are mixing constants generated offline.
  // They are not really 'magic', they just happen to work well
  uint32_t seed = dict_hash_function_seed;
  const uint32_t m = 0x5bd1e995;
  const int r = 24;

  // Initialize the hash to a 'random' value
  uint32_t h = seed ^ len;

  // Mix 4 bytes at a time into the hash
    const unsigned char *data = (const unsigned char *)key;

    while (len >= 4) {
      uint32_t k = *(uint32_t*)data;

      k *= m;
      k ^= k >> r;
      k *= m;

      h *= m;
      h ^= k;

      data += 4;
      len -= 4;
    } 

    // Handle the last few bytes of the input array
    switch (len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] >> 8;
    case 1: h ^= data[0]; h *= m;
    };

    // Do a few final mixes of the hash to ensure the last few bytes are well-incorporated
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return (unsigned int)h;
}

/* --------------- API implementation --------------------- */

/*
 * Reset a hash table already initialize with ht_size().
 * NOTE: This function should only be called by ht_destroy().
 * 
 * T = O(1)
*/
/*
 * 重置（或初始化）给定哈希表的各项属性值
 * 
 * 注意：上面的英文注释已过期
*/
static void _dictReset(dictht *ht) {
    ht->table = NULL;
    ht->size = 0;
    ht->sizemask = 0;
    ht->used = 0;
}

// Create a new hash table
// 创建一个新的字典
// T = O(1)
dict *dictCreate(dictType *type, void *privDataPtr) {
    dict *d = zmalloc(sizeof(*d));

    _dictInit(d, type, privDataPtr);

    return d;
}