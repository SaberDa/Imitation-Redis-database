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
 * 为对象的引用计数 -1
 * 
 * 当对象的引用计数降为 0 时，释放对象
*/
void decrRefCount(robj *o) {
    if (o->refcount <= 0) redisPanic("decrRefCount against refcount <= 0");

    // 释放对象
    if (o->refcount == 1) {
        switch(o->type) {
            case REDIS_STRING: 
        }
    }
}