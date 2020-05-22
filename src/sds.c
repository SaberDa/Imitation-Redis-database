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

/*
 * 在不释放 sds 的字符串空间的前提下
 * 重置 sds 保存的字符串为空
 * 
 * T = O(n)
 * 
 * Modify an sds string on-place to make it empty (zero length)
 * However all the existing buffer is not discarded but set as free space
 * so that next append operations will not require allocations up to the
 * number of bytes previously available 
*/
void sdsclear(sds s) {

    // 取出 sdshdr
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));

    // 重新计算 free 和 len
    sh->free += sh->len;
    sh->len = 0;

    // 将结束符放到最前面（相当于惰性的删除buf中的内容）
    sh->buf[0] = '\0';
}

/*
 * 对 sds 中 buf 的长度进行扩展，确保在函数执行之后，
 * buf 至少会有 addlen + 1 长度空间
 * （额外的1字节是为 \0 准备的）
 * 
 * 返回值：
 *  sds:  扩展成功则返回扩展后的sds
 *        扩展失败返回NULL
 * 
 * T = O(n)
 * 
 * Enlarge the free space at the end of the sds string so that caller 
 * is sure that after calling this function can overwrite up to addlen
 * bytes after the end of the string, plus one more byte for nul term
 * 
 * Note: this does not change the length of the sds string as returned 
 * by sdslen(), but only the free buffer space we have
*/
sds sdsMakeRoomFor(sds s, size_t addlen) {
    struct sdshdr *sh, *newsh;

    // 获取 s 目前的空余空间长度
    size_t free = sdsavail(s);

    size_t len, newlen;

    // s 目前剩余空间长度足够，无须进行扩展，直接返回
    if (free >= addlen) {
        return;
    }

    // 获取 s 目前已占用空间长度
    len = sdslen(s);
    sh = (void*) (s - (sizeof(struct sdshdr)));

    // s 最少需要的长度
    newlen = (len + addlen);

    // 根据新长度，为 s 分配新空间所需要的大小
    if (newlen < SDS_MAX_PREALLOC) {
        // 如果新长度小于 SDS_MAX_PREALLOC
        // 那么为它分配两倍于所需长度的空间
        newlen *= 2;
    } else {
        // 否则，分配长度为目前长度加上 SDS_MAX_PREALLOC
        newlen += SDS_MAX_PREALLOC;
    }

    // T = O(n)
    newsh = zrealloc(sh, sizeof(struct sdshdr) + newlen + 1);

    // 内存不足，分配失败，返回 NULL
    if (newlen == NULL) {
        return NULL;
    }

    // 更新 sds 的空余长度
    newsh->free = newlen - len;
    
    return newsh->buf;
}

/*
 * 回收 sds 中空余的空间
 * 回收不会对 sds 中保存的字符串内容做任何修改
 * 
 * 返回值：
 *  sds: 内存调整后的 sds
 * 
 * T = O(n)
 * 
 * Reallocate the sds string so that it has no free space at the end. The 
 * contained string remains not altered, but next concatenation operations 
 * will require a reallocation
 * 
 * After the call, the passed sds string is no longer valid and all the 
 * references must be substituted with the new pointer returned by the call 
*/
sds sdsRemoveFreeSpace(sds s) {
    struct sdshdr *sh;

    sh = (void*) (s - (sizeof(struct sdshdr)));

    // 进行内存重分配，让 buf 的长度仅仅足够保存字符串内容
    // T = O(n)
    sh = zrealloc(sh, sizeof(struct sdshdr) + sh->len + 1);

    // 设置空余空间为 0
    sh->free = 0;

    return sh->buf;
}

/*
 * 返回给定 sds 分配的内存字节数
 * 
 * T = O(1)
 * 
 * Return the total size of the allocation of the specifed sds string
 * including:
 * 1) The sds header before the pointer
 * 2) The string
 * 3) The free buffer at the end if any
 * 4) The implicit null term
*/
size_t sdsAllocSize(sds s) {
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));
    return sizeof(*sh) + sh->len + sh->free + 1;
}

/*
 * 根据 incr 参数，增加sds 的长度，缩减空余空间
 * 并将 '\0' 放到新字符串的尾端
 * 
 * 这个函数是在调用 sdsMakeRoomFor() 对字符串进行扩展，
 * 然后用户在字符串尾部写入了某些内容后
 * 用来正确更新 free 和 len 属性 
 * 
 * 如果 incr 为负数，那么对字符串进行右截断操作
 * 
 * T = O(1)
 * 
 * increment the sds length and decrements the left free space at the 
 * end of the string according to 'incr'. Also set the null term in the 
 * new end of the string
 * 
 * This function is used in order to fix the string length after the 
 * user calls sdsMakeRoomFor(), writes something after the end of the 
 * current string, and finally needs to set the length
 * 
 * Note: it is possible to use a negative increment in order to 
 * right-trim the string
 * 
 * Usage example:
 * 
 * Using sdsIncrLen() and sdsMakeRoomFor() it is possible to mount the 
 * following schems, to cat bytes coming from the kernel to the end of an
 * sds string without copying into an intermediate buffer:
 * 
 * oldlen = sdslen(s);
 * s = sdsMakeRoomFor(s, BUFFER_SIZE);
 * nread = read(fp, s + oldlen, BUFFER_SIZE);
 * ... check for nread <= 0 and handle it ...
 * sdsIncrLen(s, nread); 
*/
void sdsIncrLen(sds s, int incr) {
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));

    // 确保 sds 空间足够
    assert(sh->free >= incr);

    // 更新属性
    sh->len += incr;
    sh->free -= incr;

    // 这个assert可以忽略
    // 因为前一个assert已经确保 sh->free - incr >= 0 了
    assert(sh->free >= 0);

    // 放置新的结尾符
    s[sh->len] = '\0';
}
