#include <math.h>
#include <ctype.h>
#include <string.h>

#include "redis.h"

/*
 * 释放字符串对象
*/
void freeStringObject(robj *o) {
    if (o->encoding == REDIS_ENCODING_RAW) {
        sdsfree(o->ptr);
    }
}

/*
 * 释放列表对象
*/
void freeListObject(robj *o) {
    switch (o->encoding) {
        case REDIS_ENCODING_LINKEDLIST:
            listRelease((list*) o->ptr);
            break;
        case REDIS_ENCODING_ZIPLIST:
            zfree(o->ptr);
            break;
        default:
            redisPanic("Unknown list encoding type");
    }
}

/*
 * 释放集合对象
*/
void freeSetObject(robj *o) {
    switch (o->encoding) {
        case REDIS_ENCODING_HT:
            dictRelease((dict*) o->ptr);
            break;
        case REDIS_ENCODING_INTSET:
            zfree(o->ptr);
            break;
        default:
            redisPanic("Unknown set encoding type");
    }
}

/*
 * 释放有序集合对象
*/
void freeZsetObject(robj *o) {
    zset *zs;

    switch (o->encoding) {
        case REDIS_ENCODING_SKIPLIST:
            zs = o->ptr;
            dictRelease(zs->dict);
            zslFree(zs->zsl);
            zfree(zs);
            break;
        case REDIS_ENCODING_ZIPLIST:
            zfree(o->ptr);
            break;
        default:
            redisPanic("Unknown sorted set encoding");
    }
}

/*
 * 释放哈希对象
*/
void freeHashObject(robj *o) {
    switch (o->encoding) {
        case REDIS_ENCODING_HT:
            dictRelease((dict*) o->ptr);
            break;
        case REDIS_ENCODING_ZIPLIST:
            zfree(o->ptr);
            break;
        default:
            redisPanic("Unknown hash encoding type");
            break;
    }
}

/*
 * 为对象的引用计数 +1
*/
void incrRefCount(robj *o) {
    o->refcount++;
}

/*
 * 为对象的引用计数 -1
 * 
 * 当对象的引用计数降为 0 时，释放对象
*/
void decrRefCount(robj *o) {
    if (o->refcount <= 0) redisPanic("decrRefCount against refcount <= 0");

    // 若对象计数为 1 ，释放对象
    if (o->refcount == 1) {
        switch(o->type) {
            case REDIS_STRING: freeStringObject(o); break;
            case REDIS_LIST: freeListObject(o); break;
            case REDIS_SET: freeSetObject(o); break;
            case REDIS_ZSET: freeZsetObject(o); break;
            case REDIS_HASH: freeHashObject(o); break;
            default: redisPanic("Unknown object type"); break;
        }
        zfree(o);
    } else {
    // 减少计数
        o->refcount--;
    }
}

/*
 * Compare two string objects via strcmp() or strcoll() depending on flags.
 * 
 * Note that the objects may be integer-encoded. In such a case we use
 * ll2string() to get a string representation of the numbers on the 
 * stack and compare the strings, it's much faster than calling 
 * getDecodedObject().
 * 
 * Important node: when REDIS_COMPARE_BINARY is used a binary-safe 
 * comparison is used.
*/
/*
 * 根据 flags 的值，决定是否使用 strcmp() 或者 strcoll() 来对比字符串
 * 
 * 注意，因为字符串对象可能实际上保存的是整数值
 * 如果出现这种情况，那么函数先将整数转换成字符串
 * 然后再对比两个字符串
 * 这种做法比调用 getDecodedObject() 更快
 * 
 * 当 flags 为 REDIS_COMPARE_BINARY 时，
 * 对比以二进制安全的方式进行
*/
#define REDIS_COMPARE_BINARY (1 << 0)
#define REDIS_COMPARE_COLL (1 << 1)

int compareStringObjectWithFlags(robj *a, robj *b, int flags) {
    redisAssertWithInfo(NULL, a, a->type == REDIS_STRING && b->type == REDIS_STRING);
    
    char bufa[128], bufb[128], *astr, *bstr;
    size_t alen, blen, minlen;

    if (a == b) return 0;

    // 指向字符串值，并在有需要时，将整数转换为字符串 a
    if (sdsEncodedObject(a)) {
        astr = a->ptr;
        alen = sdslen(astr);
    } else {
        alen = ll2string(bufa, sizeof(bufa), (long) a->ptr);
        astr = bufa;
    }

    // 同样处理字符串 b
    if (sdsEncodedObject(b)) {
        bstr = b->ptr;
        blen = sdslen(bstr);
    } else {
        blen = ll2string(bufb, sizeof(bufb), (long) b->ptr);
        bstr = bufb;
    }

    // 对比
    if (flags & REDIS_COMPARE_COLL) {
        return strcoll(astr, bstr);
    } else {
        int cmp;
        minlen = (alen < blen) ? alen : blen;
        cmp = memcmp(astr, bstr, minlen);
        if (cmp == 0) return alen - blen;
        return cmp;
    }
}