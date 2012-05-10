/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_table_balance.cpp
 *
 *  Created on: 2011-12-9
 *      Author: wentong
 */
#include "tdh_socket_table_balance.hpp"
#include "tdh_socket_config.hpp"
#include "tdh_socket_share.hpp"
#include "easy_define.h"

#define TDHS_GUESS_TABLE_NUM (64 * 1024)

namespace taobao {

static unsigned long long total_execute_count;

static unsigned long long table_execute_count[TDHS_GUESS_TABLE_NUM];

static uint64_t quick_hash_key = 0;

static uint64_t slow_hash_key = 0;

static uint64_t round_hash_key = 0;

uint64_t table_need_balance(tdhs_request_t& req, tdhs_optimize_t type,
		uint32_t custom_hash) {
	unsigned int compare_num = tdhs_thread_num; //TODO 需要优化~
	unsigned long long total_count;
	unsigned long long table_count;
	//MARK 针对request type 需要进行判断
	if (req.type == REQUEST_TYPE_GET || req.type == REQUEST_TYPE_COUNT) {
		uint64_t hash = req.table_info.hash_code_table();
		table_count = ++table_execute_count[hash % TDHS_GUESS_TABLE_NUM];
		total_count = ++total_execute_count;
		if (type == TDHS_QUICK && custom_hash > 0
				&& thread_strategy == TDHS_THREAD_LV_1) {
			return custom_hash;
		}
		if (total_count / table_count < compare_num) {
			if (type == TDHS_QUICK) {
				hash = easy_hash_key(quick_hash_key++);
			} else {
				hash = easy_hash_key(slow_hash_key++);
			}
		}
		if (type == TDHS_SLOW) {
			//在这里控制线程数的分配
			hash %= active_slow_read_thread_num;
			//保证hash不为0 否则会rehash
			hash = hash == 0 ? active_slow_read_thread_num : hash;
		}
		return hash;
	} else if (req.type == REQUEST_TYPE_INSERT) {
		return tdhs_concurrency_insert ?
				(custom_hash > 0 ? custom_hash : easy_hash_key(round_hash_key++)) :
				req.table_info.hash_code_table();

	} else if (req.type == REQUEST_TYPE_UPDATE) {
		return tdhs_concurrency_update ?
				(custom_hash > 0 ? custom_hash : easy_hash_key(round_hash_key++)) :
				req.table_info.hash_code_table();

	} else if (req.type == REQUEST_TYPE_DELETE) {
		return tdhs_concurrency_delete ?
				(custom_hash > 0 ? custom_hash : easy_hash_key(round_hash_key++)) :
				req.table_info.hash_code_table();

	} else if (req.type == REQUEST_TYPE_BATCH) {
		return easy_hash_key(round_hash_key++);
	} else {
		return req.table_info.hash_code_table();
	}
}

} // namespace taobao

