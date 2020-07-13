#include <math.h>
#include <ctype.h>

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

    // 释放对象
    if (o->refcount == 1) {
        switch(o->type) {
            case REDIS_STRING: freeStringObject(o); break;
            case REDIS_LIST: freeListObject(0); break;
            case REDIS_SET: 
        }
    }
}