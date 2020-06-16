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
list *listAddNodeHead(list *list, void *value);
list *listAddNodeTail(list *list, void *value);
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