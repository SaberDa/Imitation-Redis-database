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

    if (init) {
        sh = zmalloc(sizeof(struct sdshdr) + initlen + 1);
    } else {
        sh = zcalloc(sizeof(struct sdshdr) + initlen + 1);
    }

    if (sh == NULL) {
        return NULL;
    }

    sh->len = initlen;
    sh->free = 0;

    if (initlen && init) {
        memcpy(sh->buf, init, initlen);
    }

    sh->buf[initlen] = '\0';

    return (char*)sh->buf;
}