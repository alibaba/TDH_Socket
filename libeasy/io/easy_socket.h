/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_SOCKET_H_
#define EASY_SOCKET_H_

#include <easy_define.h>
#include "easy_io_struct.h"
#include "easy_log.h"

/**
 * socket处理
 */

EASY_CPP_START

int easy_socket_listen(easy_addr_t *address);
int easy_socket_write(int fd, easy_list_t *l);

int easy_socket_non_blocking(int fd);
int easy_socket_set_tcpopt(int fd, int option, int value);
int easy_socket_set_opt(int fd, int option, int value);

EASY_CPP_END

#endif
