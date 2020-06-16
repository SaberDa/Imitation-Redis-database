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

typedef struct list {

    listNode *head;             // 头结点
    listNode *tail;             // 尾结点

    void *(*dup)(void *ptr);    // 结点值复制函数
    void (*free)(void *ptr);    // 结点值释放函数
    int (*match)(void *ptr, void *key);     // 结点值比较函数

    unsigned long len;          // 链表所包含的结点数量

} list;


#endif /* __ADLIST_H__ */