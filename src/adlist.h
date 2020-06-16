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

typedef struct listIter {

    listNode *next;             // 当前迭代到的结点
    int direction;              // 迭代的方向

} listIter;



#endif /* __ADLIST_H__ */