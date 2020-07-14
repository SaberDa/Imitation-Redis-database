#ifndef __REDIS_H__
#define __REDIS_H__

#include "sds.h"
#include "adlist.h"
#include "dict.h"
#include "zskiplist.h"
#include "zmalloc.h"
#include "util.h"

/* We can print the stacktrace, so our assert is defined this way: */
#define redisAssertWithInfo(_c,_o,_e) ((_e)?(void)0 : (_redisAssertWithInfo(_c,_o,#_e,__FILE__,__LINE__),_exit(1)))
#define redisAssert(_e) ((_e)?(void)0 : (_redisAssert(#_e,__FILE__,__LINE__),_exit(1)))
#define redisPanic(_e) _redisPanic(#_e,__FILE__,__LINE__),_exit(1)

/* Object types */
// 对象类型
#define REDIS_STRING 0
#define REDIS_LIST 1
#define REDIS_SET 2
#define REDIS_ZSET 3
#define REDIS_HASH 4

/* Objects encoding. Some kind of objects like Strings and Hashes can be
 * internally represented in multiple ways. The 'encoding' field of the object
 * is set to one of this fields for this object. */
// 对象编码
#define REDIS_ENCODING_RAW 0     /* Raw representation */
#define REDIS_ENCODING_INT 1     /* Encoded as integer */
#define REDIS_ENCODING_HT 2      /* Encoded as hash table */
#define REDIS_ENCODING_ZIPMAP 3  /* Encoded as zipmap */
#define REDIS_ENCODING_LINKEDLIST 4 /* Encoded as regular linked list */
#define REDIS_ENCODING_ZIPLIST 5 /* Encoded as ziplist */
#define REDIS_ENCODING_INTSET 6  /* Encoded as intset */
#define REDIS_ENCODING_SKIPLIST 7  /* Encoded as skiplist */
#define REDIS_ENCODING_EMBSTR 8  /* Embedded sds string encoding */


#define sdsEncodedObject(objptr) (objptr->encoding == REDIS_ENCODING_RAW || objptr->encoding == REDIS_ENCODING_EMBSTR)

/* The actual Redis Object */
/*
 * Redis 对象
*/
#define REDIS_LRU_BITS 24
#define REDIS_LRU_CLOCK_MAX ((1 << REDIS_LRU_BITS) - 1)
#define REDIS_LRU_CLOCK_RESOLUTION 1000
typedef struct redisObject {

    unsigned type:4;                // 类型

    unsigned encoding:4;            // 编码
    
    unsigned lur:REDIS_LRU_BITS;    // 对象最后一次被访问的时间

    int refcount;                   // 引用计数

    void *ptr;                      // 指向实际值的指针
} robj;

/*
 * 有序集合
*/
typedef struct zset {
    // 字典，键为成员，值为分值
    // 用于支持 O(1) 复杂度的按成员取分值操作
    dict *dict;

    // 跳跃表，按分值排序成员
    // 用以支持平均复杂度为 O(log(N)) 的按分值定位成员操作
    // 以及范围操作
    zskiplist *zsl;
} zset;



#endif //ifndef __REDIS_H__