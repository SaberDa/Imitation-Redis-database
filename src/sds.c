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

/*
 * 根据给定字符串 init，创建一个包含同样字符串的 sds
 * 
 * init：
 *      如果输入为 NULL，那么创建一个空白sds
 *      否则，新创建的 sds 中包含和 init 内容相同的字符串
 * 
 * 返回值：
 *  sds: 创建成功返回 sdshdr 相对应的 sds
 *       创建失败返回 NULL
 * 
 * T = O(n)
 * 
 * Create a new sds string starting from a null termind C string
*/
sds sdsnew(const char *init) {
    size_t initlen = (init == NULL) ? 0 : strlen(init);
    return sdsnewlen(init, initlen);
}

/*
 * 复制给定的 sds 副本
 * 
 * 返回值：
 *  sds： 创建成功返回输入 sds 的副本
 *        创建失败返回 NULL
 * 
 * T = O(n)
 * 
 * Duplicate an sds string
*/
sds sdsdup(const sds s) {
    return sdsnewlen(s, sdslen(s));
}

/*
 * 释放给定的 sds
 * 
 * T = O(n)
 * 
 * Free an sds string. No operation is performed if 's' is NULL
*/
void sdsfree(sds s) {
    if (s == NULL) {
        return;
    }
    zfree(s - sizeof(struct sdshdr));
}

/*
 * Set the sds string length to the length as obtained with strlen(), so
 * considering as content only up to the first null term character
 * 
 * This function is useful when the sds string is hacked manually in some
 * way, like in the following example:
 * 
 * s = sdsnew("foobar");
 * s[2] = '\0';
 * sdsupdatelen(s);
 * printf("%d\n", sdslen(s));
 * 
 * The output will be 2, but if we comment out the call to sdsupdatelen() 
 * the output will be 6 as the string was modified but the logical length 
 * remains 6 bytes
*/
void sdsupdatelen(sds s) {
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));
    int reallen = strlen(s);
    sh -> free += (sh->len - reallen);
    sh->len = reallen;
}