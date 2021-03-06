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

/*
 * 返回迭代器当前所指向的结点
 * 
 * 删除当前结点是允许的，但是不能改变链表内的其他结点
 * 
 * 函数要么返回一个结点，要么返回 NULL
*/
/*
 * Retrun the next element of an iterator.
 * It's valid to remove the currently returned element using 
 * listDelNode(), but not to remove other elements
 * 
 * The function returns a pointer to the next element of the list, 
 * or NULL if there are no more elements, so the classical usage patter
 * is:
 * 
 * iter = listGetIterator(list, <direction>);
 * while ((node = listNext(iter)) != NULL) {
 *      doSomethingWith(listNodeValue(node));
 * }
 * 
 * T = O(1)
*/
listNode *listNext(listIter *iter) {

    // 初始化指向当前结点指针的结点
    listNode *current = iter->next;

    if (current != NULL) {
        // 根据方向选择下一个结点
        if (iter->direction == AL_START_HEAD) 
            // 保存下一个结点，房子当前结点被删除而造成指针丢失
            iter->next = current->next;
        else 
            // 保存下一个结点，房子当前结点被删除而造成指针丢失
            iter->next = current->prev;
    }

    return current;
}

/*
 * 复制整个链表
 * 
 * 复制成功返回输入链表的副本，
 * 如果因为内存不足而造成复制失败，返回 NULL
 * 
 * 如果链表有设置复制函数 dup，那么对值的复制将使用复制函数进行
 * 否则，新结点和旧结点共享一个指针
 * 
 * 无论复制是成功还是失败，输入结点都不会被改变
*/
/*
 * Duplicate the whole list. On out of memory NULL is returned.
 * On success a copy of the original list is returned.
 * 
 * The 'Dup' method set with listSetDupMethod() function is used
 * to copy the node value. Otherwise the same pointer value of 
 * the original node is used as value of the copied node
 * 
 * The original list both on success or error is never modified.
 * 
 * T = O(N)
*/
list *listDup(list *orig) {

    list *copy;
    listIter *iter;
    listNode *node;

    // 创建新链表
    if ((copy = listCreate()) == NULL) return NULL;

    // 设置结点值处理函数
    copy->dup = orig->dup;
    copy->free = orig->free;
    copy->match = orig->match;

    // 迭代整个输入链表
    iter = listGetIterator(orig, AL_START_HEAD);
    while ((node = listNext(iter)) != NULL) {
        void *value;

        // 复制结点值到新结点
        if (copy->dup) {
            value = copy->dup(node->value);
            if (value == NULL) {
                listRelease(copy);
                listReleaseIterator(iter);
                return NULL;
            }
        } else {
            value = node->value;
        }

        // 将结点添加到链表
        if (listAddNodeTail(copy, value) == NULL) {
            listRelease(copy);
            listReleaseIterator(iter);
            return NULL;
        }
    }

    // 释放迭代器
    listReleaseIterator(iter);

    // 返回副本
    return copy;
}

/*
 * 查找链表 list 中值和 key 匹配的结点
 * 
 * 对比操作由链表的 match 函数进行负责进行，
 * 如果没有设置 match 函数，
 * 那么直接通过对比值的指针来决定是否匹配
 * 
 * 如果匹配成功，那么第一个匹配的结点会被返回
 * 如果失败，则返回 NULL
*/
/*
 * Search the list for a node matching a given key.
 * The match is performed using the 'match' method
 * set with listSetMatchMethod(). If no 'match' method
 * is set, the 'value' pointer of every node is directly 
 * compared with the 'key' pointer
 * 
 * On success the first matching node pointer is returned
 * (search starts from head). If no matching node exists 
 * NULL is returned
 * 
 * T = O(N)
*/
listNode *listSearchKey(list *list, void *key) {
    
    listIter *iter;
    listNode *node;

    // 迭代整个链表
    iter = listGetIterator(iter, AL_START_HEAD);
    while ((node = listNext(iter)) != NULL) {

        // 对比
        if (list->match) {
            if (list->match(node->value, key)) {
                listReleaseIterator(iter);
                // 找到
                return node;
            }
        } else {
            if (key == node->value) {
                listReleaseIterator(iter);
                // 找到
                return node;
            }
        }
    }

    listReleaseIterator(iter);

    // 未找到
    return NULL;
}

/*
 * 返回链表在给定索引上的值
 * 
 * 索引为 0 为起始，也可以是负数，-1 表示链表的最后一个结点，类推
 * 
 * 如果索引超出范围，返回NULL
*/
/*
 * Return the element at the specified zero-based index
 * where 0 is the head, 1 is the element next to head
 * and so on. Negative integers are used in order to count
 * from the tail, -1 is the last element, -2 is the penultimate 
 * and so on. If the index os out of range NULL is returned
 * 
 * T = O(N)
*/
listNode *listIndex(list *list, long index) {
    listNode *node;
    
    if (index < 0) {
        // 如果索引为负数，从表尾开始查找
        index = (-index) - 1;
        node = list->tail;
        while (index-- && node) node = node->prev;
    } else {
        // 如果索引为正数，从表头开始查找
        node = list->head;
        while (index-- && node) node = node->next;
    }

    return node;
}

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

/*
 * 将迭代器的方向设置为 AL_START_TAIL
 * 并将迭代器指针重新指向表尾结点
*/
/*
 * Create an iterator in the list private iterator structure
 * 
 * T = O(1)
*/
void listRewindTail(list *list, listIter *li) {
    li->next = list->tail;
    li->direction = AL_START_TAIL;
}

/*
 * 取出链表的表尾结点，并将它移动打表头，成为新的表头结点
*/
/*
 * Rotate the list removing the tail node and inserting it to the end
 * 
 * T = O(1)
*/
void listRotate(list *list) {
    listNode *tail = list->tail;

    if (listLength(list) <= 1) return;

    /* Detach current tail */
    // 取出表尾结点
    list->tail = tail->prev;
    list->tail->next = NULL;

    /* Move it as head */ 
    // 插入到表头
    list->head->prev = tail;
    tail->prev = NULL;
    tail->next = list->head;
    list->head = tail;
}