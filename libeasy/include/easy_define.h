/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_DEFINE_H_
#define EASY_DEFINE_H_

/**
 * 定义一些编译参数
 */

#ifdef __cplusplus
# define EASY_CPP_START extern "C" {
# define EASY_CPP_END }
#else
# define EASY_CPP_START
# define EASY_CPP_END
#endif

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <stddef.h>
#include <inttypes.h>
#include <unistd.h>
#include <execinfo.h>

///////////////////////////////////////////////////////////////////////////////////////////////////
// define
#define likely(x)                   __builtin_expect(!!(x), 1)
#define unlikely(x)                 __builtin_expect(!!(x), 0)
#define easy_align_ptr(p, a)        (uint8_t*)(((uintptr_t)(p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))
#define easy_align(d, a)            (((d) + (a - 1)) & ~(a - 1))
#define easy_max(a,b)               (a > b ? a : b)
#define easy_min(a,b)               (a < b ? a : b)

#define EASY_OK                     0
#define EASY_ERROR                  (-1)
#define EASY_ABORT                  (-2)
#define EASY_ASYNC                  (-3)
#define EASY_BREAK                  (-4)
#define EASY_AGAIN                  (-EAGAIN)
///////////////////////////////////////////////////////////////////////////////////////////////////
// typedef
typedef struct easy_addr_t {
    uint64_t addr;
    uint64_t cidx;
} easy_addr_t;

#endif
