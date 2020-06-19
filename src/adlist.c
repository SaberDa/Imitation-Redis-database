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


list *listInsertNode(list *list, listNode *old_node, void *value, int after) {
    listNode *node;

    // 新建结点
    if ((node = zmalloc(sizeof(*node))) == NULL) {
        return NULL;
    }

    // 保存值
    node->value = value;

    if (after) {
        // 将新结点添加到给定结点之后
        node->prev = old_node;
        node->next = old_node->next;
        // 给定结点时原表尾结点
        if (list->tail == old_node) {
            list->tail = node;
        }
    } else {
        // 将新结点添加到给定结点之前
        node->next = old_node;
        node->prev = old_node->prev;
        // 给定结点是原表头结点
        if (list->head == old_node) {
            list->head = node;
        }
    }

    // 更新新结点的前置指针
    if (node->prev != NULL) {
        node->prev->next = node;
    }

    // 更新新结点的后置指针
    if (node->next != NULL) {
        node->next->prev = node;
    }

    // 更新链表节点数
    list->len++;

    return list;
}

/*
 * 从链表 list 中删除给定结点 node
 * 对结点私有值的释放操作由调用者进行
*/
/*
 * Remove the specified node from the specified list
 * It's up to the caller to free the private value of the node
 * 
 * T = O(1)
*/
void listDelNode(list *list, listNode *node) {

    // 调整前置结点指针
    if (node->prev) {
        node->prev->next = node->next;
    } else {
        list->head = node->next;
    }

    // 调整后置结点指针
    if (node->next) {
        node->next->prev = node->prev;
    } else {
        list->tail = node->prev;
    }

    // 释放值
    if (list->free) {
        list->free(node->value);
    }

    // 释放结点
    zfree(node);

    // 更新链表结点数
    list->len--;
}

/*
 * 为给定链表创建一个迭代器
 * 之后每次对这个迭代器调用 listNext 都返回迭代到的链表结点
 * 
 * direction 参数决定了迭代器的迭代方向
 *  AL_START_HEAD 0     从表头向表尾进行迭代
 *  AL_START_TAIL 1     从表尾向表头进行迭代
*/
/*
 * Returns a list iterator 'iter'. After the initialization every
 * call to listNext() will return the next element of the list
 * 
 * The function can't fail
 * 
 * T = O(1)
*/
listIter* listGetIterator(list *list, int direction) {

    // 初始化迭代器
    listIter *iter;
    if ((iter = zmalloc(sizeof(*iter))) == NULL) return NULL;

    // 根据迭代方向，设置迭代器的起始结点
    if (direction == AL_START_HEAD) {
        iter->next = list->head;
    } else {
        iter->next = list->tail;
    }

    // 记录迭代方向
    iter->direction = direction;

    return iter;
}

/*
 * 释放迭代器
*/
/*
 * Release the iterator
 * 
 * T = O(1)
*/
void listReleaseIterator(listIter *iter) {
    zfree(iter);
}

listNode *listNext(listIter *iter);

list *listDup(list *orig);
listNode *listSearchKey(list *list, void *key);
listNode *listIndex(list *list, listIter *li);

/*
 * 将迭代器的方向设置为 AL_START_HEAD
 * 并将迭代器指针重新指向头结点
*/
/*
 * Create an iterator in the list private iterator structure
 * 
 * T = O(1)
*/
void listRewind(list *list, listIter *li) {
    li->next = list->head;
    li->direction = AL_START_HEAD;
}
void listRewindTail(list *list, listIter *li);
void listRotate(list *list);