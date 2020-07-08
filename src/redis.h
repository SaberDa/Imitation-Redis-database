

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