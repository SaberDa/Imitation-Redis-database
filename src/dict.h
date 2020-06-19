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

/* 哈希表
 * 每个字典都是用两个哈希表，从而实现渐进式 rehash
 */
/* This is our hash table structure. Every dictionary has two of this as we 
 * implement incremental rehashing, for the old to the new table.
*/
typedef struct dictht {

    // 哈希表数组
    dictEntry **table;

    // 哈希表大小
    unsigned long size;

    // 哈希表大小掩码，用于计算索引值
    // 总是等于 size - 1
    unsigned long sizemask;

    // 该哈希表已有结点的数量
    unsigned long used;

} dictht;

/* 字典 */
typedef struct dict {

    // 类型特定函数
    dictType *type;

    // 私有数据
    void *privdata;

    // 哈希表
    dictht ht[2];

    // rehash 索引
    // 当 rehash 不在进行时，值为 -1
    int rehashidx;      // rehashing not in process if rehashidx == -1

    // 目前正在运行的安全迭代器数量
    int iterators;      // number of iterators currently running

} dict;

/*
 * 字典迭代器
 * 
 * 如果 safe 的属性是 1，那么在迭代器进行的过程中，
 * 程序仍然可以执行 dictAdd, dictFind 和其他函数，对字典进行修改
 * 
 * 如果 safe 的属性不是 1，那么程序只会调用 dictNext 对字典进行迭代，
 * 而不是对字典进行修改
*/
/* 
 * If safe is set to 1 this is a safe iterator, that means, you can call
 * dictAdd, dictFind, and other functions against the dictionary even while
 * iterating. Otherwise it is a non safe iterator, and only dictNext() 
 * should be called while iterating.
*/
typedef struct dictIterator {

    dict *d;                // 被迭代的字典

    int table;              // 正在被迭代的哈希表号码，值可以是 0 或者 1
    int index;              // 迭代器当前所指向的哈希表索引位置
    int safe;               // 标识这个迭代器是否安全

    dictEntry *entry;       // 当前迭代到的结点的指针
    dictEntry *nextEntry;   // 当前迭代结点的下一个指针
                            // 因为在安全迭代器运作时，entry 指向的结点可能会被修改
                            // 所以需要一个额外的指针来保存下一个结点的位置
                            // 从而防止指针丢失

    long long fingerprint;  // unsafe iterator fingerprint for misuse detection

} dictIterator;

#endif /* __DICT_H */