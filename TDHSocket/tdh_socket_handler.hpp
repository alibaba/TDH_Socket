/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_handler.hpp
 *
 *  Created on: 2011-9-2
 *      Author: wentong
 */

#ifndef TDH_SOCKET_HANDLER_HPP_
#define TDH_SOCKET_HANDLER_HPP_

#include <easy_io.h>

namespace taobao {
extern int on_server_io_process(easy_request_t *r);

extern int on_server_process(easy_request_t *r, void *args);

extern int on_server_connect(easy_connection_t *c);

extern int on_server_disconnect(easy_connection_t *c);

extern void *request_thread_start(void* args);

extern void *slow_request_thread_start(void* args);

extern void *write_request_thread_start(void* args);

extern int cleanup_request(easy_request_t *r, void *apacket);

} // namespace taobao

#endif /* TDH_SOCKET_HANDLER_HPP_ */
