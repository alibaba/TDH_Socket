/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_request_thread.hpp
 *
 *  Created on: 2011-11-25
 *      Author: wentong
 */

#ifndef TDH_SOCKET_REQUEST_THREAD_HPP_
#define TDH_SOCKET_REQUEST_THREAD_HPP_

#include "easy_baseth_pool.h"

namespace taobao {
extern easy_thread_pool_t *tdhs_thread_pool_create_ex(easy_io_t *eio, int cnt,
		easy_baseth_on_start_pt *start, easy_request_process_pt *cb,
		void *args);

extern void wakeup_request_thread(easy_thread_pool_t * thread_pool);

} // namespace taobao

#endif /* TDH_SOCKET_REQUEST_THREAD_HPP_ */
