/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_share.hpp
 *
 *  Created on: 2011-10-11
 *      Author: wentong
 */

#ifndef TDH_SOCKET_SHARE_HPP_
#define TDH_SOCKET_SHARE_HPP_

#include "easy_baseth_pool.h"
#include "tdh_socket_define.hpp"

namespace taobao {

extern easy_thread_pool_t *request_server_tp;

extern easy_thread_pool_t *slow_read_request_server_tp;

extern easy_thread_pool_t *write_request_server_tp;

typedef struct tdhs_share_t {
	int shutdown;
} tdhs_share_t;

extern tdhs_share_t* tdhs_share;

typedef enum {
	TDHS_THREAD_LV_1 = 1, //只有QUICK 线程池工作
	TDHS_THREAD_LV_2, //QUICK SLOW线程池一起工作
	TDHS_THREAD_LV_3
//根据请求 可能走缓存的 走QUICK线程池  可能走IO的走SLOW线程池
} tdhs_thread_strategy_t;

extern tdhs_thread_strategy_t thread_strategy;

//当前活动的slow线程数
extern unsigned int active_slow_read_thread_num;

//auth
extern tdhs_string_t* tdhs_auth_read_code_string;

extern tdhs_string_t* tdhs_auth_write_code_string;

extern int init_auth();

extern int destory_auth();

extern bool tdhs_auth_read(tdhs_string_t &str);

extern bool tdhs_auth_write(tdhs_string_t &str);

extern void reset_auth_read_code();

extern void reset_auth_write_code();

extern void tdhs_close_cached_table();

//throttle
extern unsigned long long int slow_read_io_num;
/*return true meaning limited*/
extern bool slow_read_limit();

} // namespace taobao

#endif /* TDH_SOCKET_SHARE_HPP_ */
