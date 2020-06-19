#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"

/* Create a new list. The created list can be freed with
 * AlFreeList(), but private value of evert node need to be
 * freed by the user before to call AlFreeList().
 * 
 * Or error, NULL is returned. Otherwise the pointer to the new list.
 */
/* 创建一个新的链表
 * 创建成功返回链表，失败返回 NULL
 * T = O(1)
 */
list *listCreate(void) {
    struct list *list;

    // 分配内存
    if ((list = zmalloc(sizeof(*list))) == NULL)
        return NULL;
    
    // 初始化属性
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;

    return list;
}

/* Free the whole list.
 * This function can't fail.
*/
/*
 * 释放整个链表，以及链表中所有结点
 * 
 * T = O(N)
*/
void listRelease(list *list) {
    unsigned long len;
    listNode *current, *next;

    current = list->head;       // 指向头结点
    len = list->len;            // 当前链表长度

    // 遍历整个链表
    while (len--) {
        next = current->next;

        // 如果有设置释放函数，执行
        if (list->free) list->free(current->value);

        // 释放结点
        zfree(current);

        current = next;
    }

    // 释放链表
    zfree(list);
}

/* Add a new node to the list, to head contaning the specified 'value' 
 * pointer as value
 * 
 * On error, NULL is returned and no operation is performed 
 * (i.e. the list remains unaltered)
 * 
 * On success the 'list' pointer you pass to the function is returned.
*/
/*
 * 将一个包含有给定值指针 value 的新结点添加到链表的表头
 * 
 * 如果为新结点分配内存出错，那么不执行任何动作，返回 NULL
 * 
 * 如果执行成功，返回传入的链表指针
 * 
 * T = O(1)
*/
list *listAddNodeHead(list *list, void *value) {
    listNode *node;

    // 为结点分配内存
    if ((node == zmalloc(sizeof(*node))) == NULL) 
        return NULL;
    
    // 保存值指针
    node->value = value;

    if (list->len == 0) {
        // 添加结点到新链表
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        // 添加结点到非空链表
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    }

    // 更新链表结点数
    list->len++;

    return list;
}

/*
 * 将一个包含给定值指针 value 的新结点添加到链表的表尾
 * 如果为新结点分配内存出错，那么不执行任何动作，仅返回NULL
 * 如果执行成功，返回传入的指针值
*/
/*
 * Add a new node to the list, to tail, containing the specified 'valie'
 * pointer as value
 * 
 * On error, NULL is returned and no operation is performed 
 * (i.e. the list remains unaltered)
 * On success the 'list' pointer you pass to the function is returned
 * 
 * T = O(1)
*/
list *listAddNodeTail(list *list, void *value) {
    listNode *node;

    // 为新结点分配内存
    if ((node == zmalloc(sizeof(*node))) == NULL)
        return NULL;
    
    // 保存值指针
    node->value = value;

    // 目标链表为空
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    } else {
        // 目标链表非空
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }

    // 更新链表结点数
    list->len++;

    return list;
}
list *listInsertNode(list *list, listNode *old_node, void *value, int after);
void listDelNode(list *list, listNode *node);
listIter* listGetIterator(list *list, int direction);
listNode *listNext(listIter *iter);
void listReleaseIterator(listIter *iter);
list *listDup(list *orig);
listNode *listSearchKey(list *list, void *key);
listNode *listIndex(list *list, listIter *li);
void listRewind(list *list, listIter *li);
void listRewindTail(list *list, listIter *li);
void listRotate(list *list);