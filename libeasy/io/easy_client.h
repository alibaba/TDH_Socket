/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef easy_client_H_
#define easy_client_H_

#include <easy_define.h>
#include "easy_io_struct.h"

/**
 * 主动连接管理
 */

EASY_CPP_START

#define EASY_CLIENT_DEFAULT_TIMEOUT 5000

void *easy_client_list_find(easy_hash_t *table, easy_addr_t *addr);
int easy_client_list_add(easy_hash_t *table, easy_addr_t *addr, easy_hash_list_t *list);

EASY_CPP_END

#endif
