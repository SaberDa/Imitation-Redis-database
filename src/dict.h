/* Hash Table Implentation
 * This file implements in-memory hash tables with insert/del/replace/find/
 * get-random-element operations. Hash tables will auto-resize if needed
 * tables of power of two in size are used, collisions are handled by chaining.
 * 
 * 这个文件实现了一个内存哈希表
 * 它支持插入、删除、替换、查找和获得随机元素等操作
 * 
 * 哈希表会自动在表的大小的二次方之间进行调整
 * 
 * 键的冲突通过链表解决
 */

#include <stdint.h>

#ifndef __DICT_H
#define __DICT_H

/* 字典的操作状态 */
#define DICT_OK 0       // 操作成功
#define DICT_ERR 1      // 操作失败

/* Unused arguments generator annoying warnings */
// 如果字典的私有数据不使用时，用这个宏避免编译器错误
#define DICT_NOTUSED(V) ((void) V)

/* 哈希表结点 */
typedef struct dictEntry {

    // 键
    void *key;

    // 值
    union {
        void *val;
        uint64_t u64;
        int64_t s64;
    } v;

    // 指向下个哈希表结点，形成链表
    struct dictEntry *next;

} dictEntry;

/* 字典类型特定函数 */
typedef struct dictType {

    // 计算哈希值
    unsigned int (*hashFunction)(const void *key);

    // 复制键
    void *(*keyDup)(void *privdata, const void *key);

    // 复制值
    void *(*valDup)(void *privdata, const void * obj);

    // 对比键
    int (*ketCompare)(void *privdata, const void *key1, const void *key2);

    // 销毁键
    int (*ketDestructor)(void *privdata, void *key);

    // 销毁值
    void (*valDestructor)(void *privdata, void *obj);

} dictType;

#endif /* __DICT_H */