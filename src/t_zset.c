/*-----------------------------------------------------------------------------
 * Sorted set API
 *----------------------------------------------------------------------------*/
/*
 * ZSETs are ordered sets using two data structures to hold the same
 * elements in order to get O(log(N)) INSERT and REMOVE operations 
 * into a sorted data structure
 * 
 * The elements are added to a hash table mapping Redis objects to 
 * scores. At the same time the element are added to a skip list 
 * mapping scores to Redis objects (so objects are stored by scores
 * in this "view")
*/
/*
 * ZSET 同时使用两种数据结构来持有同一个元素
 * 从而提供 O(log(N)) 复杂度的有序数据结构的插入和移除操作
 * 
 * 哈希表将 Redis 对象映射到分值上，
 * 而跳跃表则将分值映射到 Redis 对象上
 * 以跳跃表的视角来看，可以说 Redis 对象是根据分值来排序的
*/


/*
 * The skiplist implementation is almost a C translation of the 
 * original algorithm described by William Pugh in "Skip List : A 
 * Probabilistic Alternative to Banlanced Trees", modified in 
 * three ways:
 * 
 * a) This implementation allows for repeated scores
 * 
 * b) the comparison is not just by key (our 'score') but by 
 * satellite data.
 * 
 * c) there is a back pointer, so it's a doubly linked list with 
 * the back pointers being only at "level 1". This allows to traverse 
 * the list from tail to head, useful for ZREVRANGE
*/
/*
 * Redis 的跳跃表实现和 william path 的论文中
 * 描述中的跳跃表基本相同，不过在以下三个地方做了修改：
 * 
 * 1. 这个实现允许有重复的分值
 * 
 * 2. 对元素的对比不仅要对比他们的分值，还要对比他们的对象
 * 
 * 3. 每个跳跃表结点都带有一个后退指针，
 *    它允许执行类似 ZREVRANGE 这样的操作，从表尾向表头遍历跳跃表
*/

#include "redis.h"
#include "object.c"

#include <math.h>

static int zslLexValueGteMin(robj *value, zlexrangespec *spec);
static int zslLexValueLteMax(robj *value, zlexrangespec *spec);

/*
 * 创建一个层数为 level 的跳跃表结点
 * 并将结点的成员对象设置为 obj，分值设置为 score
 * 
 * 返回值为新创建的跳跃表结点
 * 
 * T = O(1)
*/
zskiplistNode *zslCreatNode(int level, double score, robj *obj) {
    zskiplistNode *zn = zmalloc(sizeof(*zn) + level * sizeof(struct zskiplistLevel));
    zn->score = score;
    zn->obj = obj;

    return zn;
}

/*
 * 创建并返回一个新的跳跃表
 * 
 * T = O(1)
*/
zskiplist *zslCreate(void) {
    int j;
    zskiplist *zsl;

    // 分配内存
    zsl = zmalloc(sizeof(*zsl));

    // 设置高度和起始层数
    zsl->level = 1;
    zsl->length = 0;

    // 初始化表头结点
    // T= O(1)
    zsl->header = zslCreatNode(ZSKIPLIST_MAXLEVEL, 0, NULL);
    for (j = 0; j < ZSKIPLIST_MAXLEVEL; j++) {
        zsl->header->level[j].forward = NULL;
        zsl->header->level[j].span = 0;
    }
    zsl->header->backward = NULL;

    // 设置表尾结点
    zsl->tail = NULL;

    return zsl;
}

/*
 * 释放给定的跳跃表结点
 * 
 * T = O(1)
*/
void zslFreeNode(zskiplistNode *node) {
    decrRefCount(node->obj);
    zfree(node);
}

/*
 * 释放给定跳跃表，以及表中的所有结点
 * 
 * T = O(N)
*/
void zslFree(zskiplist *zsl) {
    zskiplistNode *node = zsl->header->level[0].forward, *next;

    // 释放表头
    zfree(zsl->header);

    // 释放表中所有结点
    // T = O(N)
    while (node) {
        next = node->level[0].forward;
        zslFreeNode(node);
        node = next;
    }

    // 释放跳跃表结构
    zfree(zsl);
}