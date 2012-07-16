/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_config.cpp
 *
 *  Created on: 2011-8-11
 *      Author: wentong
 */

#include "tdh_socket_config.hpp"

namespace taobao {

unsigned int tdhs_log_level = 3;

unsigned int tdhs_io_thread_num = 4;

unsigned int tdhs_thread_num = 16;

unsigned int tdhs_slow_read_thread_num = 28;

unsigned int tdhs_write_thread_num = 1;

char tdhs_cache_table_on = 1;

unsigned int tdhs_cache_table_num_for_thd = 3;

char tdhs_group_commit = 1;

int tdhs_group_commit_limits = 0;

//optimize
char tdhs_optimize_on = 0;

unsigned int tdhs_optimize_bloom_filter_group = 5;

unsigned int tdhs_optimize_bloom_filter_num_buckets = 16 * 1024;

int tdhs_optimize_guess_hot_request_num = 500 * 10000;

//optimize end

int tdhs_listen_port = 9999;

unsigned int tdhs_monitor_interval = 5;

unsigned int tdhs_thread_strategy_requests_lv_1 = 1000;

unsigned int tdhs_thread_strategy_requests_lv_2 = 12 * 1024;

char tdhs_concurrency_insert = 0;

char tdhs_concurrency_update = 0;

char tdhs_concurrency_delete = 0;

char tdhs_auth_on = 0;

char* tdhs_auth_read_code = 0;

char* tdhs_auth_write_code = 0;

//throttle
char tdhs_throttle_on = 0;

unsigned int tdhs_slow_read_limits = 2;

//extra
unsigned int tdhs_write_buff_size = 8 * 1024;

unsigned int tdhs_quick_request_thread_task_count_limit = 0;

unsigned int tdhs_slow_request_thread_task_count_limit = 0;

unsigned int tdhs_write_request_thread_task_count_limit = 0;

} // namespace taobao

