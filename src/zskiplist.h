/* ZSETs use a specialized version of Skiplists */
/*
 * 跳跃表结点
*/

#ifndef __ZSKIPLIST_H__
#define __ZSKIPLIST_H__

#include <stdio.h>

#include "redis.h"
#include "dict.h"

#define ZSKIPLIST_MAXLEVEL 32 /* Should be enough for 2^32 elements */
#define ZSKIPLIST_P 0.25      /* Skiplist P = 1/4 */

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

typedef struct {

    // 最小值和最大值 
    double min, max;                

    // 指示最小值和最大值是否 “不” 包含在范围之内
    // 值为 1 表示不包含，0 表示包含
    int minex, maxex;               

} zrangespec;


/*
 * Struct to hold an inclusive/exclusive range spec by 
 * lexicographic comparison
*/
typedef struct {
    robj *min, *max;        // May by set to shared. (minstring | maxstring)
    int minex, maxex;       // are min or max exclusive?
} zlexrangespec;

zskiplist *zslCreate(void);
void zslFree(zskiplist *zsl);
zskiplistNode *zslInsert(zskiplist *zsl, double score, robj *obj);
int zslDelete(zskiplist *zsl, double score, robj *obj);
void zslDeleteNode(zskiplist *zsl, zskiplistNode *x, zskiplistNode **update);
zskiplistNode *zslFirstInRange(zskiplist *zsl, zrangespec *range);
zskiplistNode *zslLastInRange(zskiplist *zsl, zrangespec *range);
unsigned long zslGetRank(zskiplist *zsl, double score, robj *o);
unsigned long zslDeleteRangeByScore(zskiplist *zsl, zrangespec *range, dict *dict);
unsigned long zslDeleteRangeByRank(zskiplist *zsl, unsigned int start, unsigned int end, dict *dict);

#endif