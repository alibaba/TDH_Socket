/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_REQUEST_H_
#define EASY_REQUEST_H_

#include <easy_define.h>
#include "easy_io_struct.h"

/**
 * 一个request对象
 */

EASY_CPP_START

void easy_request_server_done(easy_request_t *r);
void easy_request_client_done(easy_request_t *r);

EASY_CPP_END

#endif

