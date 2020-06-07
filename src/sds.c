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

/*
 * Grow the sds to have the specified length. Bytes that were not part of 
 * the original length of the sds will be set to zero
 * 
 * if the specified length is smaller than the current length, no operation
 * is performed
 * 
 * 将sds扩充到指定长度，未使用的空间以 0 字节填充
 * 
 * 返回值：
 *  sds: 扩充成功返回新的 sds，失败返回NULL
 * 
 * T = O(N)
*/
sds sdsgrowzero(sds s, size_t len) {
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));
    size_t totlen, curlen = sh->len;

    // 如果len比字符串的现有长度小
    // 那么直接返回，不做动作
    if (len <= curlen) {
        return s;
    }

    // 扩展 sds
    // T = O(N)
    s = sdsMakeRoomFor(s, len-curlen);

    // 如果内存不足，则直接返回
    if (s == NULL) {
        return NULL;
    }

    // Make sure added region doesn't contain garbage
    // 将新分配的空间用0填充，防止出现垃圾内容
    // T = O(N)
    sh = (void*) (s - (sizeof(struct sdshdr)));
    memset(s + curlen, 0, (len - curlen + 1));

    // 更新属性
    totlen = sh->len + sh->free;
    sh->len = len;
    sh->free = totlen - sh->len;

    // 返回新的sds
    return s;
}

/*
 * 将长度为 len 的字符串 t 追加到 sds 的字符串末尾
 * 
 * 返回值：
 *  sds: 追加成功返回新的sds，失败返回NULL
 * 
 * T = O(N)
 * 
 * Append the specified binary-safe string pointed by 't' of 'len' bytes
 * to the end of the specified sds string 's'
 * 
 * After the call, the passed sds string is no longer valid and all the 
 * reference must be substituted with the new pointer returned by the call
*/

sds sdscalen(sds s, const void *t, size_t len) {
    struct sdshdr *sh;

    // 原先字符串长度
    size_t curlen = sdslen(s);

    // 扩展 sds 空间
    // T = O(n)
    s = sdsMakeRoomFor(s, len);

    // 内存不足则直接返回NULL
    if (s == NULL) {
        return NULL;
    }

    // 复制 t 中内容到字符串后部
    // T = O(n)
    sh = (void*) (s - (sizeof(struct sdshdr)));
    memcpy(s + curlen, t, len);

    // 更新属性
    sh->len = curlen + len;
    sh->free = sh->free - len;

    // 添加新的结尾符
    s[curlen + len] = '\0';

    // 返回新的 sds
    return s;
}

/*
 * 将给定字符串 t 追加到 sds 的末尾
 * 
 * 返回值：
 *  sds：追加成功返回新 sds， 失败返回NULL
 * 
 * T= O(N)
 * 
 * Append the specified null termianted C string to the sds string 's'
 * 
 * After the call, the passed sds string is no longer valid and all the 
 * reference must be substituted with the new pointer returned by the call
*/
sds sdscat(sds s, const char *t) {
    return sdscalen(s, t, strlen(t));
}

/*
 * 将另一个 sds 追加到一个 sds 的末尾
 * 
 * 返回值：
 *  sds：追加成功返回新的 sds，失败返回NULL
 * 
 * T = O(N)
 * 
 * Append the specified sds 't' to the existing sds 's'
 * 
 * After the call, the modified sds string is no longer valid and all 
 * the references must be substituted with the new pointer returned
 * by the call 
*/
sds sdscatsds(sds s, const sds t) {
    return sdscalen(s, t, sdslen(t));
}

/*
 * 将字符串 t 的前 len 个字符复制到 sds 中
 * 并在字符串的最后添加终结符
 * 
 * 如果 sds 的长度少于 len 个字符，则扩展 sds
 * 
 * 返回值：
 *  sds : 复制成功返回新的sds，失败则返回 NULL
 * 
 * T = O(N)
 * 
 * Destructively modify the sds string 's' to hold the specificed binary
 * safe string pointed by 't' of length of 'len' bytes
*/
sds sdscpylen(sds s, const char *t, size_t len) {
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));

    // sds 现有的长度
    size_t totlen = sh->len + sh->free;

    // 如果 s 的 buf 长度不满足 len，则扩展
    if (totlen < len) {
        // T = O(N)
        s = sdsMakeRoomFor(s, len - sh->len);
        if (s == NULL) {
            return NULL;
        }
        sh = (void*) (s - (sizeof(struct sdshdr)));
        totlen = sh->free + sh->len;
    }

    // 复制内容
    // T= O(N)
    memcpy(s, t, len);

    // 添加终结符
    s[len] = '\0';

    // 更新属性
    sh->len = len;
    sh->free = totlen - len;

    // 返回新的 sds
    return s;
}

/*
 * 将字符串复制到 sds 当中
 * 覆盖原有的字符
 * 
 * 如果 sds 的长度少于字符串的长度，那么扩展 sds
 * 
 * 返回值
 *  sds: 成功返回新的 sds，失败返回NULL
 * 
 * T = O(N)
 * 
 * Like sdscpylen(), but 't' must be a null-termined string so that the length
 * of the string is obtained with strlen()
*/
sds sdscpy(sds s, const char *t) {
    return sdscpylen(s, t, strlen(t));
}

/*
 * Helper for sdscatlonglogn() doing the actual number -> string conversion
 * 's' must point to a string with room for at least SDS_LLSTR_SIZE bytes
 * 
 * The function returns the length of the null-terminated string representation 
 * stored at 's'
 * 
 * return the generated string length
 */
#define SDS_LLSTR_SIZE 21
int sdsll2str(char *s, long long value) {
    char *p;
    char aux;
    unsigned long long v;
    size_t l;

    // Generate the string representation, this method produces
    // an reversed string
    v = (value < 0) ? -value : value;
    p = s;
    do {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while (v);
    if (value < 0) {
        *p++ = '-';
    }

    // Compute length and add null term
    l = p - s;
    *p = '\0';

    // Reverse the string
    p--;
    while (s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/*
 * Identical sdsll2str(), but for unsigned long long type
 * 
 * return the generated string length
*/
int sdsull2str(char *s, unsigned long long v) {
    char *p;
    char aux;
    size_t l;

    // Generate the string representation, this method produces
    // an reversed string
    p = s;
    do {
        *p++ = '0' + (v % 10);
        v /= 10;
    } while (v);

    // Compute the length and add null term
    l = p - s;
    *p = '\0';

    // Reverse the string
    p--;
    while (s < p) {
        aux = *s;
        *s = *p;
        *p = aux;
        s++;
        p--;
    }
    return l;
}

/*
 * Create an sds string from a long long value.
 * It is much master than:
 * 
 * sdscatprintf(sdsempty(), "%lld\n", value);
 * 
 * 根据输入的 long long 值，创建一个 sds
*/
sds sdsfromlonglong(long long value) {
    char buf[SDS_LLSTR_SIZE];
    int len = sdsll2str(buf, value);

    return sdsnewlen(buf, len);
}

/*
 * 打印函数，被 sdscatprintf 调用
 * 
 * T = O(N^2)
 * 
 * Like sdscatprintf() but gets va_list instead of being variadic
*/
sds sdscatvprintf(sds s, const char *fmt, va_list ap) {
    va_list cpy;
    char staticbuf[1024], *buf = staticbuf, *t;
    size_t buflen = strlen(fmt) * 2;

    // We try to start with using a static buffer for speed
    // If not possible we revert to heap allocation
    if (buflen > sizeof(staticbuf)) {
        buf = zmalloc(buflen);
        if (buf == NULL) {
            return NULL;
        }
    } else {
        buflen = sizeof(staticbuf);
    }

    // Try with buffers two times bigger every time we fail to 
    // fit the string in the current buffer size
    while (1) {
        buf[buflen - 2] = '\0';
        va_copy(cpy, ap);
        // T = O(N)
        vsnprintf(buf, buflen, fmt, cpy);
        if (buf[buflen - 2] != '\0') {
            if (buf != staticbuf) {
                zfree(buf);
            }
            buflen *= 2;
            buf = zmalloc(buflen);
            if (buf == NULL) {
                return NULL;
            }
            continue;
        }
        break;
    }

    // Finally concat the obtained string to the SDS string and return it
    t = sdscat(s, buf);
    if (buf != staticbuf) {
        zfree(buf);
    }
    return t;
}

/*
 * 打印任意数量个字符串，并将这些字符串追加到给定 sds 的末尾
 * 
 * T = O(N^2)
 * 
 * Append to the sds string 's' a string obtained using printf-alike format
 * specifier.
 * 
 * After the call, the modified sds string is no longer valid and all the 
 * references must be substituted with the new pointer returned by the call.
 * 
 * Example:
 * s = sdsempty("Sum is: ");
 * s = sdscatprintf(s, "%d + &d = %d", a, b, a+b);
 * 
 * Often you need to create a string from scratch with the printf-alike
 * format, When this is need, just use sdsempty() as the target string:
 * 
 * s = sdscatprintf(sdsempty(), "... your format ...", args);
*/
sds sdscatprintf(sds s, const char *fmt, ...) {
    va_list ap;
    char *t;
    va_start(ap, fmt);
    // T = O(N)
    t = sdscatvprintf(s, fmt, ap);
    va_end(ap);
    return t;
}

/*
 * This function is similar to sdscatprintf, but much faster as it does
 * not rely on sprintf() family functions implemented by the libc that 
 * are often very slow. Moreover directly handling the sds string as new 
 * data is concatenated provides a performance improvement.
 * 
 * However, this function only handles an incompatible subset of 
 * printf-alike format specifiers
*/
sds sdscatfmt(sds s, char const *fmt, ...) {
    struct sdshdr* sh = (void*) (s - sizeof(struct sdshdr));
    size_t initlen = sdslen(s);
    const char *f = fmt;
    int i;
    va_list ap;

    va_start(ap, fmt);
    // Next format specifier byte to process
    f = fmt;
    // Position of the next byte to write to dest str
    i = initlen;
    while (*f) {
        char next;
        char *str;
        size_t l;
        long long num;
        unsigned long long unum;

        // Make sure there  is always space for at least 1 char
        if (sh->free == 0) {
            s = sdsMakeRoomFor(s, l);
            sh = (void*) (s - sizeof(struct sdshdr));
        }

        switch(*f) {
            case '%' :
                next = *(f + 1);
                f++;
                switch(next) {
                    case 's':
                    case 'S':
                        str = va_arg(ap, char*);
                        l = (next == 's') ? strlen(s) : sdslen(str);
                        if (sh->free < l) {
                            s = sdsMakeRoomFor(s, l);
                            sh = (void*) (s - sizeof(struct sdshdr));
                        }
                        memcpy(s + i, str, l);
                        sh->len += l;
                        sh->free -= l;
                        i += l;
                        break;
                    case 'i':
                    case 'I':
                        if (next == 'i') {
                            num = va_arg(ap, int);
                        } else {
                            num = va_arg(ap, long long);
                        }
                        {
                            char buf[SDS_LLSTR_SIZE];
                            l = sdsll2str(buf, num);
                            if (sh->free < l) {
                                s = sdsMakeRoomFor(s, l);
                                sh = (void*) (s - (sizeof(struct sdshdr)));
                            }
                            memcpy(s+i, buf, l);
                            sh->len += l;
                            sh->free -= l;
                            i += l;
                        }
                        break;
                    case 'u':
                    case 'U':
                        if (next == 'u') {
                            unum = va_arg(ap, unsigned int);
                        } else {
                            unum = va_arg(ap, unsigned long long);
                        }
                        {
                            char buf[SDS_LLSTR_SIZE];
                            l = sdsll2str(buf, unum);
                            if (sh->free < l) {
                                s = sdsMakeRoomFor(s, l);
                                sh = (void*) (s - (sizeof(struct sdshdr)));
                            }
                            memcpy(s+i, buf, l);
                            sh->len += l;
                            sh->free -= l;
                            i += l;
                        }
                        break;
                    default:
                        // Handle %% and generally %<unknown>
                        s[i++] = next;
                        sh->len += 1;
                        sh->free -= 1;
                        break;
                }
                break;
            default:
                s[i++] = *f;
                sh->len += 1;
                sh->free -= 1;
                break;
        }
        f++;
    }
    va_end(ap);

    // Add null-term
    s[i] = '\0';
    return s;
}

/*
 * 对 sds 两端进行修剪，清除其中 cset 指定的所有字符
 * 
 * 比如 sdstrim(xxyyabcyyxy, "xy") 将返回 "abc"
 * 
 * T = O(M * N)，其中 M 为 sds 长度， N 为 cset 长度
 * 
 * Remove the part of the string from left and from right composed just of
 * contiguous characters found in 'cset', that is a null terminted C string
 * 
 * After the call, the modified sds string is no longer valid and all the 
 * references must be substituted with the new pointer returned by the call
*/
sds sdstrim(sds s, const char* cset) {
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));
    char *start, *end, *sp, *ep;
    size_t len;

    // 设置和记录指针
    sp = start = s;
    ep = end = s + sdslen(s) - 1;

    // 修剪，T = O(N^2)
    while (sp <= end && strchr(cset, *sp)) sp++;
    while (ep > start && strchr(cset, *ep)) ep--;

    // 计算 trim 完毕之后剩余的字符串长度
    len = (sp > ep) ? 0 : ((ep - sp) + 1);

    // 如果有需要，前移字符串内容
    // T = O(N)
    if (sh->buf != sp) memmove(sh->buf, sp, len);

    // 添加终结符
    sh->buf[len] = '\0';

    // 更新属性
    sh->free = sh->free + (sh->len - len);
    sh->len = len;

    // 返回修剪后的 sds
    return s;
}

/*
 * 按索引对截取 sds 字符串的其中一段
 * start 和 end 都是闭区间
 * 
 * 索引从 0 开始，最大为 sdslen(s) - 1
 * 索引可以是负数，sdslen(s) - 1 == -1
 * 
 * T = O(N)
 * 
 * Turn the string into a smaller (or equal) string containing only the 
 * substring specified by the 'start' and 'end' indexes
 * 
 * start and end can be negative, where -1 means the last character of the 
 * string, -2 the penultimate character, and so forth
 * 
 * The interval is inclusive, so the start and end characters will be part 
 * of the resulting string
 * 
 * The string is modified in-place
*/
void sdsrange(sds s, int start, int end){
    struct sdshdr *sh = (void*) (s - (sizeof(struct sdshdr)));
    size_t newlen, len = sdslen(s);
    if (len == 0) {
        return;
    }
    if (start < 0) {
        start = len + start;
        if (start < 0) {
            start = 0;
        }
    }
    if (end < 0) {
        end = len + end;
        if (end < 0) {
            end = 0;
        }
    }
    newlen = (start > end) ? 0 : (end - start) + 1;
    if (newlen != 0) {
        if (start >= (signed)len) {
            newlen = 0;
        } else if (end >= (signed)len) {
            end = len - 1;
            newlen = (start > end) ? 0 : (end-start) + 1;
        }
    } else {
        start = 0;
    }

    // 如果有需要，对字符串进行移动
    // T = O(N)
    if (start && newlen) {
        memmove(sh->buf, sh->buf + start, newlen);
    }

    // add null term
    sh->buf[newlen] = 0;

    // 更新属性
    sh->free = sh->free + (sh->len - newlen);
    sh->len = newlen;
}

/*
 * 将 sds 字符串中的所有字符转换为小写
 * 
 * T = O(N)
 * 
 * Apply tolower() to every character of the sds string 's'
*/
void sdstolower(sds s) {
    int len = sdslen(s), j;
    for (j = 0; j < len; j++) {
        s[j] = tolower(s[j]);
    }
}

/*
 * 将 sds 字符串中的所有字符转换为大写
 * 
 * T = O(N)
 * 
 * Apply toupper() to every character of the sds string 's'
*/
void sdstoupper(sds s) {
    int len = sdslen(s);
    for (int j = 0; j < len; j++) {
        s[j] = toupper(s[j]);
    }
}

/*
 * 对比两个 sds, strcmp() 的 sds 版本
 * 
 * 返回值：
 *  int: 相等返回0， s1 较大返回正数，s2 较大返回负数
 * 
 * T = O(N)
 * 
 * Compare two sds strings s1 and s2 with memcpy()
 * 
 * Return value:
 *  
 *      1 if s1 > s2
 *     -1 if s1 < s2
 *      0 if s1 and s2 are exactly the same binary string
 * 
 * If two strings share exactly the same prefix, but one of the two has
 * additional characters, the longer string is considered to be greater 
 * than the smaller one
*/
int sdscmp(const sds s1, const sds s2) {
    size_t l1, l2, minlen;
    int cmp;

    l1 = sdslen(s1);
    l2 = sdslen(s2);
    minlen = (l1 < l2) ? l1 : l2;
    cmp = memcmp(s1, s2, minlen);

    if (cmp == 0) return l1 - l2;

    return cmp;
}

/*
 * 使用分隔符 sep 对 s 进行分割，返回一个 sds 字符串的数组
 * *count 会被设置为返回数组元素的数量
 * 
 * 如果出现内存不足、字符串长度为 0 或者分隔符长度为 0 的情况，
 * 返回 NULL
 * 
 * 注意分隔符可以是包含多个字符的字符串
 * 
 * 这个函数接受 len 参数，因为它是二进制安全的。
 * 文档中提到的 sdssplit() 已废弃
 * 
 * T = O(N ^ 2)
 * 
 * Split 's' with separator in 'sep'. An array of sds 
 * strings is returned. *count will be set by reference
 * to the number of tokens returned.
 * 
 * On out of memory, zero length string, zero length of 
 * separator, NULL is returned.
 * 
 * Note the 'sep' is able to split a string using a 
 * multi-character sparator. For example, 
 * sdssplit("foo_-_bar", "_-_"); will return two elements 
 * "foo" and "bar"
 * 
 * This version of the function is binary-safe but requires
 * length arguments. sdssplit() is just the same function 
 * but for zero-terminated strings
*/
sds *sdssplitlen(const char *s, int len, const char *sep, int seplen, int *count) {
    int elements = 0, slots = 0, start = 0, j;
    sds *tokens;

    if (seplen < 1 || len == 0) {
        return NULL;
    }

    tokens = zmalloc(sizeof(sds)*slots);
    if (tokens == NULL) {
        return NULL;
    }

    if (len == 0) {
        *count = 0;
        return NULL;
    }

    // T = O(N ^ 2)
    for (j = 0; j < (len - (seplen - 1)); j++) {
        // make sure there is room for the next element and the final one
        if (slots < elements + 2) {
            sds *newtoekns;

            slots *= 2;
            newtoekns - zrealloc(tokens, sizeof(sds)*slots);
            if (newtoekns == NULL) {
                goto cleanup;
            }
            tokens = newtoekns;
        }
        // search the separator
        // T = O(N)
        if ((seplen == 1 && *(s + j) == sep[0]) || (memcpy(s + j, sep, seplen) == 0)) {
            tokens[elements] == sdsnewlen(s + start, j - start);
            if (tokens[elements] == NULL) {
                goto cleanup;
            }
            elements++;
            start = j + seplen;
            j = j + seplen - 1; // skip the separator
        }
    }

    // add the final elements. We are sure there is room in the tokens array
    tokens[elements] = sdsnewlen(s + start, len - start);
    if (tokens[elements] == NULL) {
        goto cleanup;
    }
    elements++;
    *count = elements;
    return tokens;

cleanup:
    {
        int i;
        for (i = 0; i < elements; i++) {
            sdsfree(tokens[i]);
        }
        zfree(tokens);
        *count = 0;
        return NULL;
    }
}

/*
 * 释放 tokens 数组中 count 个 sds
 * 
 * T = O(N ^ 2)
 * 
 * Free the result returned by sdssplitlen(), or doing nothing
 * if 'tokens' is NULL
*/ 
void sdsfreesplitres(sds *tokens, int count) {
    if (!tokens) {
        return;
    }
    while (count--) {
        sdsfree(tokens[count]);
    }
    zfree(tokens);
}

/*
 * 将长度为 len 的字符串 p 以带引号（quoted）的格式追加
 * 到给定的 sds 末尾
 * 
 * T = O(N)
 * 
 * Append to the sds string "s" an escaped string representation where all
 * the non-printable characters (tested with isprint()) are turned into 
 * escapes in the form "\n\r\a..." or "\x<hex-number>"
 * 
 * After the call, the modified sds string is no longer valid and all the 
 * references must be substituted with the new pointer returned by the call
*/
sds sdscatrepr(sds s, const char *p, size_t len) {
    s = sdscalen(s, "\"", 1);

    while (len--) {
        switch(*p) {
            case '\\':
            case '"':
                s = sdscatprintf(s, "\\%c", *p);
                break;
            case '\n':
                s = sdscalen(s, "\\n", 2);
                break;
            case '\r':
                s = sdscalen(s, "\\r", 2);
                break;
            case '\t':
                s = sdscalen(s, "\\t", 2);
                break;
            case '\a':
                s = sdscalen(s, "\\a", 2);
                break;
            case '\b':
                s = sdscalen(s, "\\b", 2);
                break;
            default:
                if (isprint(*p)) {
                    s = sdscatprintf(s, "%c", *p);
                } else {
                    s = sdscatprintf(s, "\\x%02x", (unsigned char)*p);
                }
                break;
        }
        p++;
    }
    return sdscalen(s, "\"", 1);
}

/*
 * 如果 c 为十六进制符号中的一个，返回正数
 * 
 * Helper function for sdssplitargs() that returns non zero if 'c'
 * is a valid hex digit
 * 
 * T = O(1)
*/
int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

/*
 * 将十六进制转换为十进制
 * 
 * Helper function for sdssplitargs() that converts a hex digit into 
 * an interger from 0 to 15
 * 
 * T = O(1)
*/
int hex_digit_to_int(char c) {
    switch(c) {
        case '0' : return 0;
        case '1' : return 1;
        case '2' : return 2;
        case '3' : return 3;
        case '4' : return 4;
        case '5' : return 5;
        case '6' : return 6;
        case '7' : return 7;
        case '8' : return 8;
        case '9' : return 9;
        case 'a' : case 'A' : return 10;
        case 'b' : case 'B' : return 11;
        case 'c' : case 'C' : return 12;
        case 'd' : case 'D' : return 13;
        case 'e' : case 'E' : return 14;
        case 'f' : case 'F' : return 15;
        default: return 0;
    }
}