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
typedef struct zskiplist {

    struct zskiplistNode *header, *tail; // 表头结点和表尾结点

    unsigned long length;            // 表中结点的数量

    int level;                       // 表中层数最大的结点的层数

} zskiplist;

#endif