/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * util.hpp
 *
 *  Created on: 2011-9-2
 *      Author: wentong
 */

#ifndef UTIL_HPP_
#define UTIL_HPP_

#include <easy_log.h>
#include <stdlib.h>
#include <memory.h>

namespace taobao {

#define TAOBAO_MALLOC(s) malloc(s)
#define TAOBAO_REALLOC(p,s) realloc(p,s)
#define TAOBAO_FREE(s) free(s)

#define CACHE_MEMORY_SIZE (512)
class stack_liker {
public:
	stack_liker(size_t s) {
		if (s <= CACHE_MEMORY_SIZE) {
			memset(cache_memory, 0, sizeof(cache_memory));
			is_use_cache = true;
			ptr = cache_memory;
		} else {
			is_use_cache = false;
			if ((ptr = TAOBAO_MALLOC(s)) != NULL) {
				memset(ptr, 0, s);
			}
		}
	}
	~stack_liker() {
		if (!is_use_cache && ptr != NULL) {
			TAOBAO_FREE(ptr);
		}
	}
	void* get_ptr() {
		return ptr;
	}
private:
	char cache_memory[CACHE_MEMORY_SIZE];
	void *ptr;
	bool is_use_cache;
};

/* boost::noncopyable */
struct noncopyable {
	noncopyable() {
	}
private:
	noncopyable(const noncopyable&);
	noncopyable& operator =(const noncopyable&);
};

#define fatal_abort(M) \
		easy_fatal_log((M)); \
		abort();

#define fatal_exit(M) \
		easy_fatal_log((M)); \
		exit(-1);

#define onBit(flag,bit)   ((flag) |= (bit))
#define offBit(flag,bit)   ((flag) &= ~(bit))
#define testFlag(flag,bit)   (((flag) & (bit)) == (bit))

#define min(A,B) ((A)<(B))?(A):(B)
#define max(A,B) ((A)>(B))?(A):(B)

#define NULL_STRING ""

#define read_uint8(POS,LEN) ((LEN)<sizeof(uint8_t))? \
								({easy_warn_log("TDHS:not enough len to read!");return EASY_ERROR;0;}): \
								*((uint8_t*)(POS))

#define read_uint8_done(POS,LEN) LEN-=sizeof(uint8_t);POS+=sizeof(uint8_t)

#define read_uint8_ref(REF,POS,LEN) REF=read_uint8(POS,LEN);read_uint8_done(POS,LEN);

#define read_uint32(POS,LEN) ((LEN)<sizeof(uint32_t))? \
								({easy_warn_log("TDHS:not enough len to read!");return EASY_ERROR;0;}): \
								ntohl(*((uint32_t*)(POS)))

#define read_uint32_done(POS,LEN) LEN-=sizeof(uint32_t);POS+=sizeof(uint32_t)

#define read_uint32_ref(REF,POS,LEN) REF=read_uint32(POS,LEN);read_uint32_done(POS,LEN);

#define read_str(POS,LEN,STRLEN) ((LEN)<STRLEN)? \
									({easy_warn_log("TDHS:not enough len to read!");return EASY_ERROR;(const char*) NULL;}): \
									(STRLEN==0?NULL_STRING:(POS))

#define read_str_done(POS,LEN,STRLEN) LEN-=(STRLEN);POS+=(STRLEN);if((STRLEN!=0)&&(*(POS-1)!='\0')){easy_warn_log("TDHS:c_str error!");return EASY_ERROR;}

#define read_str_ref(REF,POS,LEN,STRLEN) REF=read_str(POS,LEN,STRLEN);read_str_done(POS,LEN,STRLEN);

} // namespace taobao

#endif /* UTIL_HPP_ */
