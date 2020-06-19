/*
 * redisassert.h
 * 
 * Drop in replacement assert.h that prints the stack tace in the Redis logs
 * 
 * This file should be included instead of "assert.h" inside libraries used by
 * Redis that are using assertions, so instead of Redis disappearing with
 * SIGABORT, we get teh ditails and stack trace inside the log file.
*/
#ifndef __REDIS_ASSERT_H__
#define __REDIS_ASSERT_H__

#include "unistd.h"     // for _exit()

#define assert(_e) ((_e) ? (void)0 : (_redisAssert(#_e, __FILE__, __LINE__), _exit(1))))

void _redisAssert(char *estr, char *file, int line);

#endif