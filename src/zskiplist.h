/* ZSETs use a specialized version of Skiplists */
/*
 * 跳跃表结点
*/

#include <stdio.h>

#include "redis.h"

#ifndef __ZSKIPLIST_H__
#define __ZSKIPLIST_H__

/* ZSETs use a specialized version of Skiplists */
// 跳跃表结点
typedef struct zskiplistNode {

    robj *obj;                      // 成员对象

    double score;                   // 分值

    struct zskiplistNode *backward; // 后退指针

    // 层
    struct zskiplistLevel {

        struct zskiplistNode *forward; // 前进指针
        
        unsigned int span;             // 跨度

    } level[];

} zskiplistNode;

// 跳跃表

#endif