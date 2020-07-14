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

/*
 * Returns a random level for the new skiplist node we are going to create
 * 
 * The return value of this functino is between 1 and ZSKIPLIST_MAXLEVEL
 * (both inclusive), with a powerlaw-alike distrbution where higher
 * levels are less likely to be returned.
 * 
 * T = O(N)
*/
/*
 * 返回一个随机值，用作新跳跃表结点的层数
 * 
 * 返回值介于 1 和 ZSKIPLIST_MAXLEVEL 之间（包含 ZSKIPLIST_MAXLEVEL），
 * 根据随机算法所使用的幂次定律，越大的值生成的几率越小
*/
int zslRandomLevel(void) {
    int level = 1;

    while ((random() & 0xFFFF) < (ZSKIPLIST_P * 0xFFFF)) {
        level += 1;
    }

    return (level < ZSKIPLIST_MAXLEVEL) ? level : ZSKIPLIST_MAXLEVEL;
}

/*
 * 创建一个成员为 obj, 分值为 score 的新结点
 * 并将这个新结点插入到跳跃表 zsl 中
 * 
 * 函数的返回值为新结点
 * 
 * T_worst = O(N ^ 2)
 * T_avg = O(Nlog(N))
*/
zskiplistNode *zslInsert(zskiplist *zsl, double score, robj *obj) {
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned int rank[ZSKIPLIST_MAXLEVEL];
    int i, level;

    redisAssert(!isnan(score));

    // 在各个层查找结点的插入位置
    // T_worst = O(N^2), T_avg = O(Nlog(N))
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {

        /* store rank that is crossed to reach the insert position */
        // 如果 i 不是 zsl->level-1 层
        // 那么 i 层的起始 rank 值为 i+1 层的 rank 值
        // 各个层的 rank 值一层层累计
        // 最终 rank[0] 的值加一就是新结点的前置结点的排位
        // rank[0] 会在后面成为计算 span 值和 rank 值的基础
        rank[i] = i == (zsl->level - 1) ? 0 : rank[i + 1];

        // 沿着前进指针遍历跳跃表
        // T_worst = O(N^2), T_avg = O(Nlog(N))
        while (x->level[i].forward && 
               (x->level[i].forward->score < score ||
                // 对比分值
                (x->level[i].forward->score == score &&
                 // 对比成员, T= O(N)
                 compareStringObjects(x->level[i].forward->obj, obj) < 0))) {
            // 记录沿途跨越了多少个结点
            rank[i] += x->level[i].span;
            // 移动至下一指针
            x = x->level[i].forward;
        }
        // 记录将要和新结点相连接的结点
        update[i] = x;
    }

    /*
     * We assume the key is not already inside, since we allow 
     * duplicated scores, and the re-insertion of score and redis
     * object should never happen since the caller of zslInsert()
     * should test in the hash table if the element is already 
     * inside or not.
     * 
     * zslInsert() 的调用者会确保同分值且同成员的元素不会出现
     * 所以这里不需要进一步检查，可以直接创建新元素
    */

    // 获取一个随机值作为新结点的层数
    // T = O(N)
    level = zslRandomLevel();

    // 如果新结点的层数比表中其他结点的层数大
    // 那么初始化表头结点中未使用的层，并将它们记录到 update 数组中
    // 将来也指向新结点
    if (level > zsl->level) {
        // 初始化未使用层
        // T = O(1)
        for (i = zsl->level; i < level; i++) {
            rank[i] = 0;
            update[i] = zsl->header;
            update[i]->level[i].span = zsl->length;
        }
        // 更新表中结点的最大层数
        zsl->length = level;
    }

    // 创建新结点
    x = zslCreatNode(level, score, obj);

    // 将前面记录的指针指向新结点，并做相应的设置
    // T = O(1)
    for (i = 0; i < level; i++) {

        // 设置新结点的 forward 指针
        x->level[i].forward = update[i]->level[i].forward;

        // 将沿途记录的各个结点的 forward 指针指向新结点
        update[i]->level[i].forward = x;

        /* update span covered by update[i] as x is inserted here */
        // 计算新结点跨越的结点数量
        x->level[i].span = update[i]->level[i].span - (rank[0] - rank[i]);

        // 更新新结点插入之后，沿途结点的 span 值
        // 其中的 +1 计算的是新结点
        update[i]->level[i].span = (rank[0] - rank[i]) + 1;
    }

    /* increment span for untouched levels */
    // 未解除的结点的 span 值也需要 +1，这些结点直接从表头指向新结点
    // T = O(1)
    for (i = level; i < zsl->level; i++) {
        update[i]->level[i].span++;
    }

    // 设置新结点的后退指针
    x->backward = (update[0] == zsl->header) ? NULL : update[0];
    if (x->level[0].forward) {
        x->level[0].forward->backward = x;
    } else {
        zsl->tail = x;
    }

    // 跳跃表中的结点计数 +1
    zsl->length++;

    return x;
}

/*
 * Internal function used by zslDelete, zslDeleteByScore and zslDeleteByRank
 * 
 * T = O(1)
*/
/*
 * 内部删除函数
*/
void zslDeleteNode(zskiplist *zsl, zskiplistNode *x, zskiplistNode **update) {
    int i;

    // 更新所有和被删除结点 x 有关的结点的指针，
    // 接触他们之间的关系
    // T = O(1)
    for (i = 0; i < zsl->level; i++) {
        if (update[i]->level[i].forward == x) {
            update[i]->level[i].span += x->level[i].span - 1;
            update[i]->level[i].forward = x->level[i].forward;
        } else {
            update[i]->level[i].span -= 1;
        }
    }

    // 更新被删除结点 x 的前进和后退指针
    if (x->level[0].forward) {
        x->level[0].forward->backward = x->backward;
    } else {
        zsl->tail = x->backward;
    }

    // 更新跳跃表最大层数
    // (只在被删除结点是跳跃表中最高的结点时才执行)
    // T = O(1)
    while (zsl->level > 1 && zsl->header->level[zsl->level - 1].forward == NULL) {
        zsl->level--;
    }

    // 跳跃表结点数 -1
    zsl->length--;
}