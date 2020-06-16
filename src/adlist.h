#ifndef __ADLIST_H__
#define __ADLIST_H__

/* Node, List, and Iterator are the only data structure used currently*/

/*
 * 双端链表结点
*/
typedef struct listNode {

    struct listNode *prev;      // 前置结点
    struct listNode *next;      // 后置结点
    void *value;                // 结点的值

} listNode;


/*
 * 双端链表迭代器
*/
typedef struct listIter {

    listNode *next;             // 当前迭代到的结点
    int direction;              // 迭代的方向

} listIter;


/*
 * 双端链表结构
*/
typedef struct list {

    listNode *head;             // 头结点
    listNode *tail;             // 尾结点

    void *(*dup)(void *ptr);    // 结点值复制函数
    void (*free)(void *ptr);    // 结点值释放函数
    int (*match)(void *ptr, void *key);     // 结点值比较函数

    unsigned long len;          // 链表所包含的结点数量

} list;


/* Function implemented as macros*/
// T = O(1)

#define listLength(l) ((l)->len)        // 返回给定链表所包含的结点数量
#define listFirst(l) ((l)->head)        // 返回给定链表的表头结点
#define listLast(l) ((l)->tail)         // 返回给定链表的表尾结点

#define listPrevNode(n) ((n)->prev)     // 返回给定结点的前置结点
#define listNextNode(n) ((n)->next)     // 返回给定结点的后置结点
#define listNodeValue(n) ((n)->value)   // 返回给定结点的值

#define listSetDupMethod(l, m) ((l)->dup = (m))     // 将链表 l 的值复制函数设置为 m
#define listSetFreeMethod(l, m) ((l)->free = (m))   // 将链表 l 的值释放函数设置为 m
#define listSetMatchMethod(l, m) ((l)->match = (m)) // 将链表 l 的值比较函数设置为 m

#define listGetDupMethod(l) ((l)->dup)              // 返回给定链表的值复制函数
#define listGetFree(l) ((l)->free)                  // 返回给定链表的值释放函数
#define listGetMatchMethod(l) ((l)->match)          // 返回给定链表的值对比函数

#endif /* __ADLIST_H__ */