#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include "sds.h"
#include "zmalloc.h"

/*
 * 根据给定的初始化字符串 init 和字符串长度 initlen
 * 创建一个新的sds
 * init: 初始化字符串指针
 * initlen: 初始化字符串长度
 * return: sds 
 * 创建成功返回sdshdr相对应的sds
 * 创建失败返回NULL
 * T = O(n)
 *
 * Create a new sds string with the content specified by the 'init' pointer
 * and 'initlen'
 * 
 * If NULL is used for 'init' the string is initialized with zero bytes
 * 
 * The string is always null-termined (all the sds strings are, always) so 
 * even if you create an sds string with:
 * 
 * mystring = sdsnewlen("abc", 3);
 * 
 * You can print the string with printf() as there si an implicit \0 at the 
 * end of the string. However, the string is binary safe and can contain \0
 * characters in the middle, as the length is stroed in the sds header
*/
sds sdsnewlen(const void *init, size_t initlen) {
    struct sdshdr *sh;
    
    // 根据是否有初始化内容，选择适当所分配的内存
    // T = O(n)
    if (init) {
        // zmalloc 不初始化所分配的内存
        sh = zmalloc(sizeof(struct sdshdr) + initlen + 1);
    } else {
        // zcalloc 将分配的内存全部初始化为0
        sh = zcalloc(sizeof(struct sdshdr) + initlen + 1);
    }

    // 内存分配失败
    if (sh == NULL) {
        return NULL;
    }

    // 设置初始化长度
    sh->len = initlen;
    // 新sds不预留任何空间
    sh->free = 0;

    // 如果有指定初始化内容，将他们复制到 sdshdr 的 buf 中
    // T = O(n)
    if (initlen && init) {
        memcpy(sh->buf, init, initlen);
    }

    // 以  \0 结尾
    sh->buf[initlen] = '\0';

    // 返回 buf 部分，而不是整个 sdshdr
    return (char*)sh->buf;
}

/*
 * 创建并返回一个只保存了空字符串 "" 的sds
 * 
 * 返回值
 *   sds：创建成功返回 sdshdr 相对应的 sds
 *        创建失败返回 NULL
 * 
 * 复杂度 
 * T = O(n)
 * 
 * Create an empty (zero length) sds string. Even in this case the string
 * always has am implicit null term
*/
sds sdsempty(void) {
    return sdsnewlen("", 0);
}