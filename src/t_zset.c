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

#include "zskiplist.h"
#include "zmalloc.h"

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