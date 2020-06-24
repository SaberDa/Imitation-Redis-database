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

/* Initialize the hash table 
 * T = O(1)
*/
/* 初始化哈希表*/ 
int _dictInit(dict *d, dictType *type, void * privDataPtr) {

    // 初始化两个哈希表的各项属性值
    // 但暂时还不分配内存给哈希表数组
    _dictReset(&d->ht[0]);
    _dictReset(&d->ht[1]);

    // 设置类型特定函数
    d->type = type;

    // 设置私有数据
    d->privdata = privDataPtr;

    // 设置哈希表 rehash 状态
    d->rehashidx = -1;

    // 设置字典的安全迭代器数量
    d->iterators = 0;

    return DICT_OK; 
}

/*
 * Resize the table to the minimal size that contains all the elements,
 * but with the invariant of a USED/BUCKETS ratio near to <= 1.
 * 
 * T = O(N)
*/
/*
 * 缩小给定字典
 * 让它的已用字节数和字典大小之间的比率接近 1:1
 * 
 * 返回 DICT_ERR 表示字典已经在 rehash, 或者 dict_can_resize 为假。
 * 
 * 成功创建体积更小的 ht[1]，可以开始 resize 时，返回 DICT_OK
*/
int dictResize(dict *d) {
    int minimal;

    // 不能在关闭 rehash 或者正在 rehash 的时候调用
    if (!dict_can_resize || dictIsRehashing(d)) return DICT_ERR;

    // 计算比率接近 1:1 所需要的最少节点数量
    minimal = d->ht[0].used;
    if (minimal < DICT_HT_INITIAL_SIZE) 
        minimal = DICT_HT_INITIAL_SIZE;
    
    //调整字典的大小
    // T= O(N)
    return dictExpand(d, minimal);
}

/* Expand or create the hash table */
/*
 * 创建一个新的哈希表，并根据字典的情况，选择以下其中一个动作来进行
 * 
 * 1) 如果字典的 0 号哈希表为空，那么将新哈希表设置为 0 号哈希表
 * 2) 如果字典的 0 号哈希表非空，那么将新哈希表设置为 1 号哈希表
 *    并打开字典的 rehash 标识，使得程序可以对字典进行 rehash
 * 
 * size 参数不够大，或者 rehash 已经在进行时，放回 DICT_ERR
 * 
 * 成功创建 0 号哈希表，或者 1 号哈希表时，返回 DICT_OK
 * 
 * T = O(N)
*/
int dictExpand(dict *d, unsigned long size) {

    // 新哈希表
    dictht n;

    // 根据 size 参数，计算哈希表的大小
    // T = O(1)
    unsigned long realsize = _dictNextPower(size);


    /* The size is invalid if it is smaller than the number of 
     * elements already inside the hash table */
    // 不能在字典正在 rehash 时进行
    // size 的值也不能小于 0 号哈希表的当前已使用结点
    if (dictIsRehashing(d) || d->ht[0].used > size) return DICT_ERR;

    /* Allocate the new hash table and initialize all pointers to NULL */
    // 为哈希表分配空间，并将所有指针指向 NULl
    n.size = realsize;
    n.sizemask = realsize - 1;
    // T = O(N)
    n.table = zcalloc(realsize * sizeof(dictEntry*));
    n.used = 0;

    /* Is this the first initialization? If so it's not really a rehashing
     * we just set the first hash table so that it can accept keys
    */
    // 如果 0 号哈希表为空，那么这是一次初始化
    // 程序将新哈希表赋给 0 号哈希表的指针，然后字典就可以开始处理键值对了 
    if (d->ht[0].table == NULL) {
        d->ht[0] = n;
        return DICT_OK;
    }

    /* Prepare a second hash table for incremental rehashing */
    // 如果 0 号哈希表非空，那么这是一次 rehash
    // 程序将新哈希表设置为 1 号哈希表
    // 并将字典的 rehash 标识打开，让程序可以开始对字典进行 rehash
    d->ht[1] = n;
    d->rehashidx = 0;
    return DICT_OK;
}

/* Perfroms N steps of incremental rehashing. Returns 1 if there are still
 * keys to move from the old to the new hash table, otherwise 0 is returned.
 * 
 * Note that a rehashing step consists in moving a bucket (that may have more 
 * than one key as we use chaining) from the old to the new hash table
 *
 * T = O(N)
*/
/*
 * 执行 N 步渐进式 rehash
 * 
 * 返回 1 表示仍有键需要从 0 号哈希表移动到 1 号哈希表
 * 返回 0 表示所有键都已经迁移完毕
 * 
 * 注意： 每步 rehash 都是以一个哈希表索引（桶）作为单位的
 * 一个桶里可以有多个结点
 * 被 rehash 的桶里的所有结点都会被移动到新哈希表
*/
int dictRehash(dict *d, int n) {
    // 只可以在 rehash 进行时执行
    if (!dictIsRehashing(d)) return 0;

    // 进行 N 步迁移
    // T = O(N)
    while (n--) {
        dictEntry *de, *nextde;

        /* check if we already rehashed the whole table */
        // 如果 0 号哈希表为空，那么表示 rehash 执行完毕
        // T = O(1)
        if (d->ht[0].used == 0) {
            // 释放 0 号哈希表
            zfree(d->ht[0].table);
            // 将原来的 1 号哈希表设置为新的 0 号哈希表
            d->ht[0] = d->ht[1];
            // 重置旧的 1 号哈希表
            _dictReset(&d->ht[1]);
            // 关闭 rehash 标识
            d->rehashidx = -1;
            // 返回 0， 向调用者表示 rehash 已经完成
            return 0;
        }

        /* Note that rehashidx can't overflow as we are sure there are more elements because ht[0].used != 0 */
        // 确保 rehashidx 没有越界
        assert(d->ht[0].size > (unsigned)d->rehashidx);

        // 略过数组中为空的索引，找到下一个非空的索引
        while (d->ht[0].table[d->rehashidx] == NULL) d->rehashidx++;

        // 指向该索引的链表头结点
        de = d->ht[0].table[d->rehashidx];

        /* Move all the keys in this bucket from the old to the new hash HT */
        // 将链表中所有结点迁移到新哈希表
        // T = O(1)
        while (de) {
            unsigned int h;

            // 保存下个结点的指针
            nextde = de->next;

            /* Get the index in the new hash table */
            // 计算新哈希表的哈希值，以及结点插入的索引位置
            h = dictHashKey(d, de->key) & d->ht[1].sizemask;

            // 插入结点到新的哈希表
            de->next = d->ht[1].table[h];
            d->ht[1].table[h] = de;
            
            // 更新计数器
            d->ht[0].used--;
            d->ht[1].used++;

            // 继续处理下个结点
            de = nextde;
        }

        // 将刚迁移完的哈希表索引的指针设为空
        d->ht[0].table[d->rehashidx] = NULL;

        // 更新 rehash 索引
        d->rehashidx++;
    }
    return 1;
}

/*
 * 返回以毫秒为单位的 UNIX 时间戳
 * 
 * T = O(1)
*/
long long timeInMilliseconds(void) {
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (((long long)tv.tv_sec) *1000) + (tv.tv_usec / 1000);
}

/* Rehash for an amount of the time between ms millisecond ans ms + 1 milliseconds */
/*
 * 在给定毫秒内，以 100 步为单位，对字典进行 rehash
 * 
 * T = O(N)
*/
int dictRehashMillisecond(dict *d, int ms) {

    long long start = timeInMilliseconds();
    int rehashes = 0;

    while (dictRehash(d, 100)) {
        rehashes += 100;
        if (timeInMilliseconds() - start > ms) break;
    }

    return rehashes;
}

/* 
 * This function preforms just a step of rehashing, and only if there are
 * no safe iterators bound to our hash table. When we have iterators in the
 * middle of a rehashing we can't mess with the two hash tables otherwise 
 * some element can be missed or duplicated
 * 
 * This function is called by common lookup or update operations in the 
 * dictionary so that the hash table automatically migrates from H1 to H2
 * while it is actively used
 * 
 * T = O(1)
 */
/*
 * 在字典不存在安全迭代器的情况下，对字典进行单步 rehash
 * 
 * 字典有安全迭代器的情况下不能进行 rehash
 * 因为两种不同的迭代和修改操作可能会弄乱字典
 * 
 * 这个函数被多个通用的查找、更新操作调用，
 * 它可以让字典在被使用的同时进行 rehash
*/
static void _dictRehashStep(dict *d) {
    if (d->iterators == 0) dictRehash(d, 1);
}