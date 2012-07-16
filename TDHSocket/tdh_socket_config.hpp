/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_config.hpp
 *
 *  Created on: 2011-8-11
 *      Author: wentong
 */

#ifndef TDH_SOCKET_CONFIG_HPP_
#define TDH_SOCKET_CONFIG_HPP_

namespace taobao {

extern unsigned int tdhs_log_level;

extern unsigned int tdhs_io_thread_num;

extern unsigned int tdhs_thread_num;

extern unsigned int tdhs_slow_read_thread_num;

extern unsigned int tdhs_write_thread_num;

extern char tdhs_cache_table_on;

extern unsigned int tdhs_cache_table_num_for_thd;

extern char tdhs_group_commit;

extern int tdhs_group_commit_limits;

//optimize

extern char tdhs_optimize_on;

extern unsigned int tdhs_optimize_bloom_filter_group;

extern unsigned int tdhs_optimize_bloom_filter_num_buckets;

extern int tdhs_optimize_guess_hot_request_num;

//optimize end

extern int tdhs_listen_port;

extern unsigned int tdhs_monitor_interval;

extern unsigned int tdhs_thread_strategy_requests_lv_1;

extern unsigned int tdhs_thread_strategy_requests_lv_2;

extern char tdhs_concurrency_insert;

extern char tdhs_concurrency_update;

extern char tdhs_concurrency_delete;

//auth
extern char tdhs_auth_on;

extern char* tdhs_auth_read_code;

extern char* tdhs_auth_write_code;

//throttle
extern char tdhs_throttle_on;

extern unsigned int tdhs_slow_read_limits;

//extra
extern unsigned int tdhs_write_buff_size;

extern unsigned int tdhs_quick_request_thread_task_count_limit;

extern unsigned int tdhs_slow_request_thread_task_count_limit;

extern unsigned int tdhs_write_request_thread_task_count_limit;

} // namespace taobao

#endif /* TDH_SOCKET_CONFIG_HPP_ */
