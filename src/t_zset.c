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

/* 
 * Delete an element with matching score/object from the skiplist
 * 
 * T_worst = O(N ^ 2)
 * T_avg = O(N log(N))
 */
/*
 * 从跳跃表 zsl 中删除班号给定结点 score 并且带有指定对象 obj 的结点
*/
int zslDelete(zskiplist *zsl, double score, robj *obj) {
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;

    // 遍历跳跃表，查找目标节点，并记录所有沿途结点
    // T_worst = O(N ^ 2), T_avg = O(N log(N))
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        // 遍历跳跃表的复杂度为  T_worst = O(N ^ 2), T_avg = O(N log(N))
        while (x->level[i].forward &&
               (x->level[i].forward->score < score ||
                // 对比分值
                (x->level[i].forward->score == score &&
                 // 对比对象, T = O(N)
                 compareStringObjects(x->level[i].forward->obj, obj) < 0))) {
            // 沿着前进指针移动
            x = x->level[i].forward;
        }
        // 记录沿途结点
        update[i] = x;
    }

    /*
     * We may have multiple elements with the same score, what we
     * need is to find the element with both the right score and object
    */
    /*
     * 检查找到的元素 x，只有在它的分值和对象都相同时，才将它删除
    */
    x = x->level[0].forward;
    if (x && score == x->score && equalStringObjects(x->obj, obj)) {
        // T = O(1)
        zslDeleteNode(zsl, x, update);
        // T = O(1)
        zslFreeNode(x);
        return 1;
    } else {
        return 0;  /* not found */
    }
    return 0;  /* not found */
}

/*
 * 检测给定值 value 是否大于（或大于等于）范围 spec 中的 min 值
 * 
 * 返回 1 表示 value 大于等于 min 项，否则返回 0
 * 
 * T = O(1)
*/
static int zslValueGteMin(double value, zrangespec *spec) {
    return spec->minex ? (value > spec->min) : (value >= spec->min);
}

/*
 * 检测给定值 value 是否小于（或小于等于）范围 spec 中的 max 值
 * 
 * 返回 1 表示 value 小于等于 max 项，否则返回 0
 * 
 * T = O(1)
*/
static int zslValueGteMax(double value, zrangespec *spec) {
    return spec->maxex ? (value < spec->max) : (value <= spec->max);
}

/*
 * Returns if there is a part of the zset is in range
 * 
 * T = O(1)
*/
/*
 * 如果给定的分值范围包含在跳跃表的分值范围之内
 * 那么返回 1，否则返回 0
*/
int zslIsInRange(zskiplist *zsl, zrangespec *range) {
    zskiplistNode *x;

    /* Test for ranges that will always be empty */
    // 先排除总为空的范围值
    if (range->min > range->max ||
        (range->min == range->max &&
         (range->minex || range->maxex))) {
        return 0;
    }

    // 检查最大分值
    x = zsl->tail;
    if (x == NULL || !zslValueGteMin(x->score, range)) {
        return 0;
    }

    // 检查最小分值
    x = zsl->header->level[0].forward;
    if (x == NULL || !zslValueGteMax(x->score, range)) {
        return 0;
    }

    return 1;
}

/*
 * Find the first node that is contained in the specified range 
 * 
 * Return NULL when no element is contained in the range 
 * T_worst = O(N)
 * T_avg = O(N log(N))
*/
/*
 * 返回 zsl 中第一个分值符合 range 中指定范围的结点
 * 
 * 如果 zsl 中没有符合范围的结点，返回 NULL
*/
zskiplistNode *zslFirstInRange(zskiplist *zsl, zrangespec *range) {
    zskiplistNode * x;
    int i;

    /* If everything is out of range, return early */
    // T = O(1)
    if (!zslIsInRange(zsl, range)) return NULL;

    // 遍历跳跃表，查找符合范围 min 项的结点
    // T_worst = O(N), T_avg = O(N log(N))
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        /* Go forward while "OUT" of range */
        while (x->level[i].forward &&
               !zslValueGteMin(x->level[i].forward->score, range)) {
            x = x->level[i].forward;
        }
    }

    /* This is an inner range, so the next node cannot be NULL */
    x = x->level[0].forward;
    redisAssert(x != NULL);

    /* Check if score <= max */
    // 检查结点是否符合范围的 max 项
    // T = O(1)
    if (!zslValueGteMax(x->score, range)) return NULL;

    return x;
}

/*
 * Find the last node that is contained in the specified range 
 * 
 * Return NULL when no element is contained in the range 
 * T_worst = O(N)
 * T_avg = O(N log(N))
*/
/*
 * 返回 zsl 中最后一个分值符合 range 中指定范围的结点
 * 
 * 如果 zsl 中没有符合范围的结点，返回 NULL
*/
zskiplistNode *zslLastInRange(zskiplist *zsl, zrangespec *range) {
    zskiplistNode *x;
    int i;

    /* If everything is out of range, return early */
    // T = O(1)
    if (!zslIsInRange(zsl, range)) return NULL;

    // 遍历跳跃表，查找符合范围 max 项的结点
    // T_worst = O(N), T_avg = O(N log(N))
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        /* Go forward while *IN* range. */
        while (x->level[i].forward &&
               zslValueGteMax(x->level[i].forward->score, range)) {
            x = x->level[i].forward;
        }
    }

    /* This is an inner range, so the next node cannot be NULL */
    redisAssert(x != NULL);

    /* Check if score >= min */
    // 检查结点是否符合范围的 min 项
    // T = O(1)
    if (!zslValueGteMin(x->score, range)) return NULL;

    return x;
}

/*
 * Delete all the elements with score between min and max from the skiplist
 * 
 * Min and Max are inclusive, so a score >= min || score <= max is deleted
 * 
 * Note that this function takes the reference to the hash table view of 
 * the sorted set, in ordered to remove the elements from the hash table too.
 * 
 * T = O(1)
*/
/*
 * 删除所有分值在给定范围之内的结点
 * 
 * min 和 max 参数都是包含在范围之内的，所以 score >= min || score <= max 的
 * 结点都会被删除
 * 
 * 结点不仅会从跳跃表中删除，而且还会从对应的字典中删除
 * 
 * 返回值为被删除结点的数量
*/
unsigned long zslDeleteRangeByScore(zskiplist *zsl, zrangespec *range, dict *dict) {
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned long removed = 0;
    int i;

    // 记录所有和被删除结点有关的结点
    // T_wrost = O(N) , T_avg = O(log N)
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && (range->minex ?
               x->level[i].forward->score <= range->min :
               x->level[i].forward->score < range->min)) {
            x = x->level[i].forward;
        }
        // 记录沿途结点
        update[i] = x;
    }

    /* Current node is the last with score < or score <= min */
    // 定位到给定范围开始的第一个结点
    x = x->level[0].forward;

    /* Delete nodes while in range */
    // 删除范围中的所有结点
    // T = O(N)
    while (x &&
           (range->maxex ? x->score < range->max : x->score <= range->max)) {
        // 记录下一个结点
        zskiplistNode *next = x->level[0].forward;
        zslDeleteNode(zsl, x, update);
        dictDelete(dict, x->obj);
        zslFreeNode(x);
        removed++;
        x = next;
    }

    return removed;
}

unsigned long zslDeleteRangeByLex(zskiplist *zsl, zlexrangespec *range, dict *dict) {
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    int i;
    unsigned long removed = 0;

    // 记录所有和被删除结点有关的结点
    // T_wrost = O(N) , T_avg = O(log N)
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
               !zslLexValueGteMin(x->level[i].forward->obj, range)) {
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    /* Current node is the last with score < or <= min. */
    x = x->level[0].forward;

    /* Delete nodes while in range. */
    // 删除范围中的所有结点
    // T = O(N)
    while (x && zslLexValueLteMax(x->obj, range)) {
        // 记录下一个结点
        zskiplistNode *next = x->level[0].forward;

        zslDeleteNode(zsl, x, update);
        dictDelete(dict, x->obj);
        zslFreeNode(x);

        removed++;
        x = next;
    }

    return removed;
}

/*
 * Delete all the elements with rank between start and end from the skiplist
 * 
 * Start and end are inclusive. Note that start and end need to be 1-based
 * 
 * T = O(N)
*/
/*
 * 从跳跃表中删除所有给定排位内的结点
 * 
 * start 和 end 两个位置都是包含在内的，注意他们都是以 1 为起始值
 * 
 * 函数的返回值为被删除结点的数量
*/
unsigned long zslDeleteRangeByRank(zskiplist *zsl, unsigned int start, unsigned int end, dict *dict) {
    zskiplistNode *update[ZSKIPLIST_MAXLEVEL], *x;
    unsigned long traversed = 0, removed = 0;
    int i;

    // 沿着前进指针移动到指定排位的起始位置，并记录所有沿途指针
    // T_wrost = O(N) , T_avg = O(log N)
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) < start) {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }
        update[i] = x;
    }

    // 移动到排位起始的第一个结点
    traversed++;
    x = x->level[0].forward;

    // 删除给定范围内的结点
    // T = O(N)
    while (x && traversed <= end) {
        // 记录下一个结点
        zskiplistNode *next = x->level[0].forward;
        zslDeleteNode(zsl, x, update);
        dictDelete(dict, x->obj);
        zslFreeNode(x);
        x = next;

        removed++;
        traversed++;
    }
    return removed;
}

/*
 * Find the rank for an element by both score and key
 * 
 * Returns 0 when the element cannot be found, rank otherwise
 * 
 * Node that the rank is 1-based due to the span of zsl->header 
 * to the first element
 * 
 * T_worst = O(N)
 * T_avg = O(logN)
*/
/*
 * 查找包含给定分值和成员对象的结点在跳跃表中的排位
 * 
 * 如果没有包含给定分值和对象成员的结点，返回 0，否则返回排位
 * 
 * 注意，因为跳跃表的表头也被计算在内，所以排位的初始值为 1
*/
unsigned long zslGetRank(zskiplist *zsl, double score, robj *o) {
    zskiplistNode *x;
    unsigned long rank = 0;
    int i;

    // 遍历整个跳跃表
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward &&
               (x->level[i].forward->score < score ||
                // 对比分值
                (x->level[i].forward->score == score &&
                 // 对比对象成员
                 compareStringObjects(x->level[i].forward->obj, o) <= 0))) {
            // 累计跨越的结点数量
            rank += x->level[i].span;
            // 沿着前进指针遍历跳跃表
            x = x->level[i].forward;
        }

        /* x might be equal to zsl->header, so that if obj is non-NULL */
        // 必须确保不仅分值相等，而且成员对象也要相等
        // T = O(N)
        if (x->obj && equalStringObjects(x->obj, o)) {
            return rank;
        }
    }
    /* not found */
    return 0;
}

/*
 * Finds an element by its rank. The rank argument needs to be 1-based
 * 
 * T_worst = O(N)
 * t_avg = O(logN)
*/
/*
 * 根据排位在跳跃表中查找元素。排位的起始位置为 1
 * 
 * 成功查找返回相应的跳跃表结点，没找到返回 NULL
*/
zskiplistNode* zslGetElementByRank(zskiplist *zsl, unsigned long rank) {
    zskiplistNode *x;
    unsigned long traversed = 0;
    int i;

    // 遍历跳跃表结点
    // T_wrost = O(N), T_avg = O(log N)
    x = zsl->header;
    for (i = zsl->level - 1; i >= 0; i--) {
        while (x->level[i].forward && (traversed + x->level[i].span) <= rank) {
            traversed += x->level[i].span;
            x = x->level[i].forward;
        }

        // 如果越过的结点数量已经等于 rank
        // 那么说明已经找到了要找的结点
        if (traversed == rank) {
            return x;
        }
    }
    /* Not found */
    return NULL;
}

/*
 * Populate the rangespec according to the objects min and max
 * 
 * T = O(N)
*/
/*
 * 对 min 和 max 分析，并将区间的值保存到 spec 中
 * 
 * 分析成功返回 REDIS_OK，分析出错导致失败返回 REDIS_ERR
*/
static int zslParseRange(robj *min, robj *max, zrangespec *spec) {
    char *eptr;

    // 默认为闭区间
    spec->minex = spec->maxex = 0;

    /*
     * Parse the min-max interval. If one of the values is prefixed
     * by the "(" character, it's considered "open". For instance, 
     * ZRANGEBYSCORE zset (1.5 (2.5 will match min < x < max
     * ZRANGEBYSCORE zset 1.5 2.5 will instead match min <= x <= max
    */ 
    if (min->encoding == REDIS_ENCODING_INT) {
        // min 的值为整数，开区间
        spec->min = (long)min->ptr;
    } else {
        // min 的对象为字符串，分析 min 的值并决定区间
        if (((char*)min->ptr)[0] == '(') {
            // 闭区间
            // T = O(N)
            spec->min = strtod((char*)min->ptr + 1, &eptr);
            if (eptr[0] != '\0' || isnan(spec->min)) return REDIS_ERR;
            spec->minex = 1;
        } else {
            // 开区间
            // T = O(N)
            spec->min = strtod((char*)min->ptr, &eptr);
            if (eptr[0] != '\0' || isnan(spec->min)) return REDIS_ERR;
        }
    }

    if (max->encoding == REDIS_ENCODING_INT) {
        // max 的值为整数，开区间
        spec->max = (long)max->ptr;
    } else {
        // max 的对象为字符串，分析 max 的值并决定区间
        if (((char*)max->ptr)[0] == '(') {
            // 开区间
            // T = O(N)
            spec->max = strtod((char*)max->ptr + 1, &eptr);
            if (eptr[0] != '\0' || isnan(spec->max)) return REDIS_ERR;
            spec->maxex = 1;
        } else {
            // 闭区间
            // T = O(N)
            spec->max = strtod((char*)max->ptr, &eptr);
            if (eptr[0] != '\0' || isnan(spec->max)) return REDIS_ERR;
        }
    }

    return REDIS_OK;
}

/* ---------------------- Lexicographic Ranges ------------------------- */

/*
 * Parse max or min argument of ZRANGEBYLEX
 * (foo means foo (open interval)
 * [foo means foo (closed interval)
 * - means the min string possible
 * + means the max string possible
 * 
 * If the string is valid the *dest pointer is set to the redis object
 * that will be used for the comparison, and ex will be set to 0 or 1
 * respectively if the item is exclusive or inclusive. REDIS_OK will be 
 * returned
 * 
 * If the string is not a valid range REDIS_ERR is returned, and the 
 * value of *dest and *ex is undefined
*/
int zslParseLexRangeItem(robj *item, robj **dest, int *ex) {
    char *c = item->ptr;

    switch (c[0]) {
        case '+':
            if (c[1] != '\0') return REDIS_ERR;
            *ex = 0;
            *dest = shared.maxstring;
            incrRefCount(shared.maxstring);
            return REDIS_OK;
        case '-':
            if (c[1] != '\0') return REDIS_ERR;
            *ex = 0;
            *dest = shared.minstring;
            incrRefCount(shared.minstring);
            return REDIS_OK;
        case '(':
            *ex = 1;
            *dest = createStringObject(c + 1, sdslen(c) - 1);
            return REDIS_OK;
        case '[':
            *ex = 0;
            *dest = createStringObject(c + 1, sdslen(c) - 1);
            return REDIS_OK;
        default:
            return REDIS_ERR;
    }
}

/*
 * Populate the rangespec according to the objects min and max
 * 
 * Return REDIS_OK on success. On error REDIS_ERR is returned.
 * When OK is returned the structure must be freed with zslFreeLexRange(),
 * otherwise no release is needed.
*/
static int zslParseLexRange(robj *min, robj *max, zlexrangespec *spec) {
    /*
     * The range can't be valid if objects are integer encoded.
     * Every item must start with ( or [.
    */
    if (min->encoding == REDIS_ENCODING_INT ||
        max->encoding == REDIS_ENCODING_INT) return REDIS_ERR;
    
    spec->min = spec->max = NULL;
    if (zslParseLexRangeItem(min, &spec->min, &spec->minex) == REDIS_ERR ||
        zslParseLexRangeItem(max, &spec->max, &spec->maxex) == REDIS_ERR) {
        if (spec->min) decrRefCount(spec->min);
        if (spec->max) decrRefCount(spec->max);
        return REDIS_ERR;
    } else {
        return REDIS_OK;
    }
}