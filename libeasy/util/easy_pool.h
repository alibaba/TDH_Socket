/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_POOL_H_
#define EASY_POOL_H_

/**
 * 简单的内存池
 */
#include <easy_define.h>
#include <easy_list.h>
#include <easy_atomic.h>
#include <easy_mem_slab.h>

EASY_CPP_START

#define EASY_POOL_ALIGNMENT         512
#define EASY_POOL_PAGE_SIZE         4096

typedef void *(*easy_pool_realloc_pt)(void *ptr, size_t size);
typedef struct easy_pool_large_t easy_pool_large_t;
typedef struct easy_pool_t easy_pool_t;

struct easy_pool_large_t {
    easy_pool_large_t       *next;
    uint8_t                 data[0];
};

struct easy_pool_t {
    uint8_t                 *last;
    uint8_t                 *end;
    easy_pool_t             *next;
    uint16_t                failed;
    uint16_t                flags;
    uint32_t                max;

    // pool header
    easy_pool_t             *current;
    easy_pool_large_t       *large;
    easy_atomic_t           ref;
    easy_atomic_t           tlock;
};

extern easy_pool_realloc_pt easy_pool_realloc;
extern void *easy_pool_default_realloc (void *ptr, size_t size);

extern easy_pool_t *easy_pool_create(uint32_t size);
extern void easy_pool_clear(easy_pool_t *pool);
extern void easy_pool_destroy(easy_pool_t *pool);
extern void *easy_pool_alloc(easy_pool_t *pool, uint32_t size);
extern void *easy_pool_nalloc(easy_pool_t *pool, uint32_t size);
extern void *easy_pool_calloc(easy_pool_t *pool, uint32_t size);
extern void easy_pool_set_allocator(easy_pool_realloc_pt alloc);
extern void easy_pool_set_lock(easy_pool_t *pool);

extern char *easy_pool_strdup(easy_pool_t *pool, const char *str);

EASY_CPP_END
#endif
