#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <unistd.h>
#include <float.h>

#include "util.h"

/* =========================================================================== */

// 手动实现 gettimeofday() 函数
// 替代 unix 下的 <sys/time.h>
#include <time.h>
#include <windows.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
    #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
    #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct timezone
{
    int  tz_minuteswest; /* minutes W of Greenwich */
    int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    FILETIME ft;
    unsigned __int64 tmpres = 0;
    static int tzflag;

    if (NULL != tv)
    {
        GetSystemTimeAsFileTime(&ft);

        tmpres |= ft.dwHighDateTime;
        tmpres <<= 32;
        tmpres |= ft.dwLowDateTime;

        /*converting file time to unix epoch*/
        tmpres /= 10;  /*convert into microseconds*/
        tmpres -= DELTA_EPOCH_IN_MICROSECS;
        tv->tv_sec = (long)(tmpres / 1000000UL);
        tv->tv_usec = (long)(tmpres % 1000000UL);
    }

    if (NULL != tz)
    {
        if (!tzflag)
        {
            _tzset();
            tzflag++;
        }
        tz->tz_minuteswest = _timezone / 60;
        tz->tz_dsttime = _daylight;
    }

    return 0;
}

/*
 * Convert a long long into a string. 
 * Returns the number of characters needed to represent the number,
 * that can be shorter if passed buffer length is not enough to 
 * store the whole number
*/
int ll2string(char *s, size_t len, long long value) {
    char buf[32], *p;
    unsigned long long v;
    size_t l;

    if (len == 0) return 0;
    v = (value < 0) ? -value : value;
    p = buf + 31;   // point to the last character
    do {
        *p-- = '0' + (v % 10);
        v /= 10;
    } while (v);
    if (value < 0) *p-- = '-';
    p++;
    l = 32 - (p - buf);
    if (l + 1 > len) l = len - 1;   // Make sure it fits, including the null term
    memcpy(s, p, l);
    s[l] = '\0';
    return l;
}