/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_HASH_H_
#define EASY_HASH_H_

/**
 * 固定HASH桶的hashtable, 需要在使用的对象上定义一个easy_hash_list_t
 */
#include "easy_pool.h"
#include "easy_list.h"

EASY_CPP_START

typedef struct easy_hash_t easy_hash_t;
typedef struct easy_hash_list_t easy_hash_list_t;
typedef int (easy_hash_cmp_pt)(const void *a, const void *b);

struct easy_hash_t {
    easy_hash_list_t    **buckets;
    uint32_t            size;
    uint32_t            mask;
    uint32_t            count;
    int                 offset;

    uint64_t            seqno;
    easy_list_t         list;
};

struct easy_hash_list_t {
    easy_hash_list_t    *next;
    easy_hash_list_t    **pprev;
    uint64_t            key;
};

#define easy_hash_for_each(i, node, table)                      \
    for(i=0; i<table->size; i++)                                \
        for(node = table->buckets[i]; node; node = node->next)

extern easy_hash_t *easy_hash_create(easy_pool_t *pool, uint32_t size, int offset);
extern int easy_hash_add(easy_hash_t *table, uint64_t key, easy_hash_list_t *list);
extern void *easy_hash_find(easy_hash_t *table, uint64_t key);
void *easy_hash_find_ex(easy_hash_t *table, uint64_t key, easy_hash_cmp_pt cmp, const void *a);
extern void *easy_hash_del(easy_hash_t *table, uint64_t key);
extern int easy_hash_del_node(easy_hash_list_t *n);
extern uint64_t easy_hash_key(uint64_t key);
extern uint64_t easy_hash_code(const void *key, int len, unsigned int seed);

extern int easy_hash_dlist_add(easy_hash_t *table, uint64_t key, easy_hash_list_t *hash, easy_list_t *list);
extern void *easy_hash_dlist_del(easy_hash_t *table, uint64_t key);

EASY_CPP_END

#endif
