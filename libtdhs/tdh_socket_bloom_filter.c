/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#include <stdlib.h>
#include <string.h>

#include "tdh_socket_bloom_filter.h"

bloom_filter* create_bfilter(size_t size) {
	bloom_filter* bFilter;
	if (!(bFilter = (bloom_filter*) malloc(sizeof(bloom_filter))))
		return NULL;
	bFilter->fsize = FSIZE(size);
	if (!(bFilter->a = malloc(bFilter->fsize))) {
		free(bFilter);
		return NULL;
	}
	memset(bFilter->a, 0, bFilter->fsize);
	bFilter->asize = size;
	return bFilter;
}

void destroy_bfilter(bloom_filter* bFilter) {
	if (bFilter == NULL) {
		return;
	}
	if (bFilter->a) {
		free(bFilter->a);
		bFilter->a = NULL;
	}
	if (bFilter) {
		free(bFilter);
		bFilter = NULL;
	}
}

void bfilter_add(const bloom_filter* bFilter, const uint64_t* input) {
	SETBIT(bFilter->a, *input % bFilter->asize);
}

int bfilter_check(const bloom_filter* bFilter, const uint64_t* input) {
	return GETBIT(bFilter->a, *input % bFilter->asize);
}

