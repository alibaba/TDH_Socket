/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_MESSAGE_H_
#define EASY_MESSAGE_H_

#include <easy_define.h>
#include "easy_io_struct.h"

/**
 * 接收message
 */

EASY_CPP_START

easy_message_t *easy_message_create(easy_connection_t *c);
void easy_message_destroy(easy_message_t *m, int del);
int easy_session_process(easy_session_t *s, int stop);
void easy_message_cleanup(easy_buf_t *b, void *args);

EASY_CPP_END

#endif
