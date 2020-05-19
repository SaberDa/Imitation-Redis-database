#ifndef __ZMALLOC_H
#define __ZMALLOC_H

#define __xstr(s) __str(s)
#define __str(s) #s   

#if defined(USE_TCMALLOC)
#define ZMALLOC_LIB ("tcmalloc-" __xstr(TC_VERSION_MAJOR) "." __xstr(TC_VERSION_MINOR))
#include <goolge/tcmalloc.h>
#if (TC_VERSION_MAJOR == 1 && TC_VERSION_MINOR >= 6) || (TC_VERSION_MAJOR >1)
#define HAVE_MALLOC_SIZE 1
#define zmalloc_size(p) tc_malloc_size(p)
#else 
#error "Newer version of tcmalloc required"
#endif

#elif defined(USE_JEMALLOC)
#define ZMALLOC_LIB ("jemalloc-" __xstr(JEMALLOC_VERSION_MAJOR) "." __xstr(JEMALLOC_VERSION_MINOR) "." __xstr(JEMALLOC_VERSION_BUGFIX))
#include <jemalloc/hemalloc.h>
#if (JEMALLOC_VERSION_MAJOR == 2 && JEMALLOC_VERSION_MINOR >= 1) || (JEMALLOC_VERSION_MAJOR > 2)
#define zmalloc_size(p) je_malloc_usable_size(p)
#else
#error "Newer version of jemalloc required"
#endif

#elif defined(__APPLE__)
#include <malloc/malloc.h>
#define HAVE_MALLOC_SIZE 1
#define zmalloc_zise(p) malloc_size(p)
#endif

#ifndef ZMALLOC_LIB
#define ZMALLOC_LIB "libc"
#endif

// 调用zmalloc函数，申请size大小的空间
void *zmalloc(size_t size);

// 调用系统函数calloc申请内存空间
void *zcalloc(size_t size);

// 原内存重新调整为size空间的大小
void *zrealloc(void *ptr, size_t size);

// 调用zfree释放内存
void zfree(void *ptr);

// 字符串复制方法
char *zstrdup(const char *s);

// 获取当前以及占用内存的空间大小
size_t zmalloc_used_memory(void);

// 是否设置线程安全模式
void zmalloc_enable_thread_safeness(void);

// 可自定义设置内存溢出的处理方式
void zmalloc_set_oom_handler(void (*oom_handler)(size_t));

// 获取所给内存和已使用内存的大小之比
float zmalloc_get_fragmentation_ratio(size_t size);

// 获取RSS信息 (Resident Set Size)
size_t zmalloc_get_rss(void);

// 获得实际内存大小
size_t zmalloc_get_private_dirty(void);

// 原始系统free释放方法
void zlibc_free(void *ptr);

#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void *ptr);
#endif

#endif