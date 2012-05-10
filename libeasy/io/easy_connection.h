/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_CONNECTION_H_
#define EASY_CONNECTION_H_

#include <easy_define.h>
#include "easy_io_struct.h"

/**
 * 连接主程序
 */

EASY_CPP_START

#define EASY_CONNECT_ADDR           1
#define EASY_DISCONNECT_ADDR        2
#define EASY_MIN_INTERVAL           0.1

typedef struct easy_connection_list_t {
    easy_connection_t   *head;
    easy_connection_t   *tail;
} easy_connection_list_t;

// fuction
easy_listen_t *easy_connection_listen_addr(easy_io_t *eio, easy_addr_t addr, easy_io_handler_pt *handler);
void easy_connection_on_wakeup(struct ev_loop *loop, ev_async *w, int revents);
void easy_connection_on_listen(struct ev_loop *loop, ev_timer *w, int revents);
int easy_connection_write_socket(easy_connection_t *c);
int easy_connection_request_process(easy_request_t *r, easy_io_process_pt *process);

int easy_connection_send_session_list(easy_list_t *list);
int easy_connection_session_build(easy_session_t *s);
void easy_connection_wakeup_session(easy_connection_t *c);
void easy_connection_destroy(easy_connection_t *c);
int easy_connection_request_done(easy_request_t *c);

EASY_CPP_END

#endif

