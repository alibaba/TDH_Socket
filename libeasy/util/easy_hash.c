/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#include "easy_hash.h"

/**
 * 创建一easy_hash_t
 */
easy_hash_t *easy_hash_create(easy_pool_t *pool, uint32_t size, int offset)
{
    easy_hash_t         *table;
    easy_hash_list_t    **buckets;
    uint32_t            n;

    // 2 ^ m
    n = 4;
    size &= 0x7fffffff;

    while(size > n) n <<= 1;

    // alloc
    buckets = (easy_hash_list_t **)easy_pool_calloc(pool, n * sizeof(easy_hash_list_t *));
    table = (easy_hash_t *)easy_pool_alloc(pool, sizeof(easy_hash_t));

    if (table == NULL || buckets == NULL)
        return NULL;

    table->buckets = buckets;
    table->size = n;
    table->mask = n - 1;
    table->count = 0;
    table->offset = offset;
    table->seqno = 1;
    easy_list_init(&table->list);

    return table;
}

int easy_hash_add(easy_hash_t *table, uint64_t key, easy_hash_list_t *list)
{
    uint64_t            n;
    easy_hash_list_t    *first;

    n = easy_hash_key(key);
    n &= table->mask;

    // init
    list->key = key;
    table->count ++;
    table->seqno ++;

    // add to list
    first = table->buckets[n];
    list->next = first;

    if (first)
        first->pprev = &list->next;

    table->buckets[n] = (easy_hash_list_t *)list;
    list->pprev = &(table->buckets[n]);

    return EASY_OK;
}

void *easy_hash_find(easy_hash_t *table, uint64_t key)
{
    uint64_t            n;
    easy_hash_list_t    *list;

    n = easy_hash_key(key);
    n &= table->mask;
    list = table->buckets[n];

    // foreach
    while(list) {
        if (list->key == key) {
            return ((char *)list - table->offset);
        }

        list = list->next;
    }

    return NULL;
}

void *easy_hash_find_ex(easy_hash_t *table, uint64_t key, easy_hash_cmp_pt cmp, const void *a)
{
    uint64_t            n;
    easy_hash_list_t    *list;

    n = easy_hash_key(key);
    n &= table->mask;
    list = table->buckets[n];

    // foreach
    while(list) {
        if (list->key == key) {
            if (cmp(a, ((char *)list - table->offset)) == 0)
                return ((char *)list - table->offset);
        }

        list = list->next;
    }

    return NULL;
}

void *easy_hash_del(easy_hash_t *table, uint64_t key)
{
    uint64_t            n;
    easy_hash_list_t    *list;

    n = easy_hash_key(key);
    n &= table->mask;
    list = table->buckets[n];

    // foreach
    while(list) {
        if (list->key == key) {
            easy_hash_del_node(list);
            table->count --;

            return ((char *)list - table->offset);
        }

        list = list->next;
    }

    return NULL;
}

int easy_hash_del_node(easy_hash_list_t *node)
{
    easy_hash_list_t    *next, **pprev;

    if (!node->pprev)
        return 0;

    next = node->next;
    pprev = node->pprev;
    *pprev = next;

    if (next) next->pprev = pprev;

    node->next = NULL;
    node->pprev = NULL;

    return 1;
}

int easy_hash_dlist_add(easy_hash_t *table, uint64_t key, easy_hash_list_t *hash, easy_list_t *list)
{
    easy_list_add_tail(list, &table->list);
    return easy_hash_add(table, key, hash);
}

void *easy_hash_dlist_del(easy_hash_t *table, uint64_t key)
{
    char                *object;

    if ((object = (char *)easy_hash_del(table, key)) != NULL) {
        easy_list_del((easy_list_t *)(object + table->offset + sizeof(easy_hash_list_t)));
    }

    return object;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// hash 64 bit
uint64_t easy_hash_key(volatile uint64_t key)
{
    void *ptr = (void *) &key;
    return easy_hash_code(ptr, sizeof(uint64_t), 5);
}

uint64_t easy_hash_code(const void *key, int len, unsigned int seed)
{
    const uint64_t m = __UINT64_C(0xc6a4a7935bd1e995);
    const int r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t *data = (const uint64_t *)key;
    const uint64_t *end = data + (len / 8);

    while(data != end) {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char *data2 = (const unsigned char *)data;

    switch(len & 7) {
    case 7:
        h ^= (uint64_t)(data2[6]) << 48;

    case 6:
        h ^= (uint64_t)(data2[5]) << 40;

    case 5:
        h ^= (uint64_t)(data2[4]) << 32;

    case 4:
        h ^= (uint64_t)(data2[3]) << 24;

    case 3:
        h ^= (uint64_t)(data2[2]) << 16;

    case 2:
        h ^= (uint64_t)(data2[1]) << 8;

    case 1:
        h ^= (uint64_t)(data2[0]);
        h *= m;
    };

    h ^= h >> r;

    h *= m;

    h ^= h >> r;

    return h;
}
