/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef TDH_SOCKET_BLOOM_FILTER_H
#define TDH_SOCKET_BLOOM_FILTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <limits.h>

#define SETBIT(a, n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GETBIT(a, n) (a[n/CHAR_BIT] & (1<<(n%CHAR_BIT)))
#define FSIZE(size) (((size + CHAR_BIT - 1) / CHAR_BIT) * sizeof(char))

typedef struct {
	size_t asize;
	unsigned char *a;
	size_t fsize;
} bloom_filter;

bloom_filter* create_bfilter(size_t size);
void destroy_bfilter(bloom_filter* bFilter);

void bfilter_add(const bloom_filter* bFilter, const uint64_t* input);

int bfilter_check(const bloom_filter* bFilter, const uint64_t* input);

#ifdef __cplusplus
}
#endif

#endif
