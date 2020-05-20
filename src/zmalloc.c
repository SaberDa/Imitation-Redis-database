#include <stdio.h>
#include <stdlib.h>

// this function provide us access to the original libc free()
// this is useful to free results obtained by backtrace_symbols()
// we need to define this function before including zmalloc.h that may shadow the free implementation if we use jemalloc or another not standard allocator
void zlibc_free(void *ptr) {
    free(ptr);
}

#include <string.h>
#include <pthread.h>
// #include "libs/headers.h"
#include "config.h"
#include "zmalloc.h"

#ifdef HAVE_MALLOC_SIZE
#define PREFIX_SIZE (0)
#else
#if defined(__sun) || defined(__sparc) || defined(__sparc__)
#define PREFIX_SIZE (sizeof(long long))
#else
#define PREFIX_SIZE (sizeof(size_t))
#endif
#endif

// explicitly override malloc/ free tec when using tcmalloc
#if defined(USE_TCMALLOC)
#define malloc(size) tc_malloc(size)
#define calloc(count,size) tc_calloc(count,size)
#define realloc(ptr,size) tc_realloc(ptr,size)
#define free(ptr) tc_free(ptr)
#elif defined(USE_JEMALLOC)
#define malloc(size) je_malloc(size)
#define calloc(count,size) je_calloc(count,size)
#define realloc(ptr,size) je_realloc(ptr,size)
#define free(ptr) je_free(ptr)
#endif

#ifdef HAVE_ATOMIC
#define update_zmalloc_stat_add(__n) __sync_add_and_fetch(&used_memory, (__n))
#define update_zmalloc_stat_sub(__n) __sync_sub_and_fetch(&used_memory, (__n))
#else
#define update_zmalloc_stat_add(__n) do { \
    pthread_mutex_lock(&used_memory_mutex); \
    used_memory += (__n); \
    pthread_mutex_unlock(&used_memory_mutex); \
} while(0)

#define update_zmalloc_stat_sub(__n) do { \
    pthread_mutex_lock(&used_memory_mutex); \
    used_memory -= (__n); \
    pthread_mutex_unlock(&used_memory_mutex); \
} while(0)

#endif

// 原子加操作
// 内存状态统计函数
// 首先将n调整为sizeof(long)的整数倍
// 如果使用了线程安全模式，则调用原子操作(+)来更新已知内存
// 若不考虑线程安全，则直接更新已知内存
#define update_zmalloc_stat_alloc(__n) do { \
    size_t _n = (__n); \
    if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
    if (zmalloc_thread_safe) { \
        update_zmalloc_stat_add(_n); \
    } else { \
        used_memory += _n; \
    } \
} while(0)

// 原子减函数
// 内存状态统计函数
// 先将内存大小调整为sizeof(long)的整数倍
// 若开启了线程安全模式，则调用原子操作(-)来更新已知内存
// 若不考虑线程安全，则直接更新已知内存
#define update_zmalloc_stat_free(__n) do { \
    size_t _n = (__n); \
    if (_n&(sizeof(long)-1)) _n += sizeof(long)-(_n&(sizeof(long)-1)); \
    if (zmalloc_thread_safe) { \
        update_zmalloc_stat_sub(_n); \
    } else { \
        used_memory -= _n; \
    } \
} while(0)


// 已使用内存大小
static size_t used_memory = 0;
// 线程安全模式状态
static int zmalloc_thread_safe = 0;
// 服务器
pthread_mutex_t used_memory_mutex = PTHREAD_MUTEX_INITIALIZER;

// 内存异常处理函数
static void zmalloc_default_oom(size_t size) {
    fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n", size);
    fflush(stderr);
    about();        // 中断退出
}

static void(*zmalloc_oom_handler)(size_t) = zmalloc_default_oom;

void *zmalloc(size_t size) {
    // 调用malloc函数进行内存申请
    // 多申请的PrREFIX_SIZE大小的内存用于记录该段内存的大小
    void *ptr = malloc(size + PREFIX_SIZE);
    // 如果ptr为空，则调用异常处理函数
    if (!ptr) zmalloc_oom_handler(size);
#ifdef HAVE_MALLOC_SIZE
    update_zmalloc_stat_alloc(zmalloc_size(ptr));
    return ptr;
#else 
    // 内存统计
    *((size_t*)ptr) = size;
    // 更新used_memory
    update_zmalloc_stat_alloc(size + PREFIX_SIZE);
    return (char*)ptr+PREFIX_SIZE;
#endif
}

// 调用calloc申请内存
void *zcalloc(size_t size) {
    void *ptr = calloc(1, size + PREFIX_SIZE);
    if (!ptr) {
        // 异常处理函数
        zmalloc_oom_handler(size);
    }
#ifdef HAVE_MALLOC_SIZE
    update_zmalloc_stat_alloc(size + PREFIX_SIZE);
    return ptr;
#else
    *((size_t*)ptr) = size;
    update_zmalloc_stat_alloc(size + PREFIX_SIZE);
    return (char*)ptr + PREFIX_SIZE;
#endif
}

// 内存调整函数
// 用于调整已申请内存的大小，本质调用了系统的recalloc()
void *zrealloc(void *ptr, size_t size) {
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
#endif
    size_t oldsize;
    void *newptr;

    // 若ptr为空，则直接退出
    if (ptr == NULL) {
        return zmalloc(size);
    }

#ifdef HAVE_MALLOC_SIZE
    oldsize = zmalloc_size(ptr);
    newptr = realloc(ptr, size);
    if (!newptr) {
        zmalloc_oom_handler(size);
    }

    update_zmalloc_stat_free(oldsize);
    update_zmalloc_stat_alloc(zmalloc_size(newptr));
    return newptr;
#else 
    // 找到内存真正的起始位置
    realptr = (char*)ptr - PREFIX_SIZE;
    oldsize = *((size_t*)realptr);
    // 调用realloc()函数
    newptr = realloc(realptr, size + PREFIX_SIZE);
    if (!newptr) {
        zmalloc_oom_handler(size);
    }
    // 内存统计
    *((size_t*)newptr) = size;
    // 先减去原先已使用内存大小
    // 然后再加上调整后的大小
    update_zmalloc_stat_free(oldsize);
    update_zmalloc_stat_alloc(size);
    return (char*)newptr +PREFIX_SIZE;
#endif
}

/*
 * Provide zmalloc_size() for systems where this function is not provided
 * by malloc itself, given that in that case we store a header with this 
 * information as the first bytes of every allocation.
*/
#ifndef HAVE_MALLOC_SIZE
size_t zmalloc_size(void *ptr) {
    void *realptr = (char*)ptr - PREFIX_SIZE;
    size_t size = *((size_t*)realptr);
    /*
     * Assume at least that all the allocations are padded at 
     * sizeof(long) by the underlying allocator
    */
    if (size & (sizeof(long) - 1)) {
        size += sizeof(long) - (size & (sizeof(long) - 1));
    }
    return size + PREFIX_SIZE;
}
#endif

// 内存释放函数
// 调用系统free()函数
void zfree(void *ptr) {
#ifndef HAVE_MALLOC_SIZE
    void *realptr;
    size_t oldsize;
#endif

    // 为空直接返回
    if (ptr == NULL) {
        return;
    }

#ifdef HAVE_MALLOC_SIZE
    update_zmalloc_stat_free(zmalloc_size(ptr));
    free(ptr);
#else 
    // 找到该段内存的起始位置
    realptr = (char*)ptr - PREFIX_SIZE;
    oldsize = *((size_t*)realptr);
    // 更新used_memory
    update_zmalloc_stat_free(oldsize + PREFIX_SIZE);
    // 释放内存
    free(realptr);
#endif
}

// 字符串复制方法
char *zstrdup(const char *s) {
    // 开辟一段新内存
    size_t l = strlen(s) + 1;
    char *p = zmalloc(l);

    // 复制字符串
    memcpy(p, s, l);
    return p;
}

// 获取已知内存
size_t zmalloc_used_memory(void) {
    size_t um;

    if (zmalloc_thread_safe) {
#ifdef HAVE_ATOMIC
// 使用GCC提供的原子操作
        um = __sync_add_and_fetch(&used_memory, 0);
#else 
// 若不支持原子操作，则使用线程锁
        pthread_mutex_lock(&used_memory_mutex);
        um = used_memory;
        pthread_mutex_unlock(&used_memory_mutex);
#endif
    } else {
        um = used_memory;
    }

    return um;
}

// 开启线程安全
void zmalloc_enable_thread_safemess(void) {
    zmalloc_thread_safe = 1;
}

// 允许自行设定异常处理函数
// 可绑定自定义的异常处理函数
void zmalloc_set_oom_handler(void (*oom_handler)(size_t)) {
    zmalloc_oom_handler = oom_handler;
}

/*
 * Get the RSS information in an OS-specific way
 * 
 * WARNING: the function zmalloc_get_rss() is not designed to be fast and
 * may not be called in the busy loops where Redis tries to release
 * memory expiring or swapping out objects
 * 
 * For this kind of "fast RSS reporting" usage use instead the function
 * RedisEstimateRSS() that is a much faster (and less precise) version
 * of the function
*/


#if defined(HAVE_PROC_STAT)
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


size_t zmalloc_get_rss(void) {
    int page = sysconf(_SC_PAGESIZE);
    size_t rss;
    char buf[4096];
    char filename[256];
    int fd, count;
    char *p, *x;

    snprintf(filename, 256, "/proc/%d/stat", getpid());
    if ((fd = open(filename, O_RDONLY)) == -1) {
        return 0;
    }
    if (read(fd, buf, 4096) < 0) {
        close(fd);
        return 0;
    }
    close(fd);

    p = buf;
    count = 23;   // RSS is the 24th field in /proc/<pid>/stat
    while (p && count--) {
        p = strchr(p, ' ');
        if (p) {
            p++;
        }
    }
    if (!p) {
        return 0;
    }
    x = strchr(p, ' ');
    if (!x) {
        return 0;
    }
    *x = '\0';

    rss = strtoll(p, NULL, 10);
    rss *= page;
    return rss;
}
#elif defined(HAVE_TASKINFO)
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/task.h>
#include <mach/mach_init.h>

size_t zmalloc_get_rss(void) {
    task_t task = MACH_PORT_NULL;
    struct task_basic_info t_info;
    mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

    if (task_for_pid(current_task(), getpid(), &task) != KERN_SUCCESS) {
        return 0;
    }
    task_info(task, TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count);

    return t_info.resident_size;
}

#else
size_t zmalloc_get_rss(void) {
    /*
     * If we can't get the RSS in an OS-specific way for this system 
     * just return the memory usage we estimated in zmalloc()
     * 
     * Fragmentation will appear to be always 1 (no fragmentation)
     * of course
    */
    return zmalloc_used_memory();
}

#endif

// Fragmentation = RSS / allocation-bytes
float zmalloc_get_fragmentation_ratio(size_t rss) {
    return (float)rss / zmalloc_used_memory();
}

#if defined(HAVE_PROC_SMAPS) 
size_t zmalloc_get_private_dirty(void) {
    char line[1024];
    size_t pd = 0;
    FILE *fp = fopen("/proc/self/smaps", "r");

    if (!fp) {
        return 0;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "Private_Dirty", 14) == 0) {
            char *p = strchr(line, 'k');
            if (p) {
                *p = '\0';
                pd += strtol(line + 14, NULL, 10) * 1024;
            }
        }
    }
    fclose(fp);
    return pd;
}

#else 

size_t zmalloc_get_private_dirty(void) {
    return 0;
}

#endif