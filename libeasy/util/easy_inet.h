/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_INET_H_
#define EASY_INET_H_

/**
 * inet的通用函数
 */
#include "easy_define.h"

EASY_CPP_START

extern char *easy_inet_addr_to_str(easy_addr_t *addr, char *buffer, int len);
extern easy_addr_t easy_inet_str_to_addr(const char *host, int port);
extern int easy_inet_parse_host(easy_addr_t *address, const char *host, int port);
extern int easy_inet_is_ipaddr(const char *host);
extern int easy_inet_hostaddr(uint64_t *address, int size);

EASY_CPP_END

#endif
