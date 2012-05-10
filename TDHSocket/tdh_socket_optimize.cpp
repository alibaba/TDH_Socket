/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_optimize.cpp
 *
 *  Created on: 2011-11-10
 *      Author: wentong
 */

#include "tdh_socket_optimize.hpp"
#include "tdh_socket_share.hpp"
#include "tdh_socket_config.hpp"
#include "tdh_socket_statistic.hpp"
#include "thread_and_lock.hpp"
#include "util.hpp"

#include "tdh_socket_bloom_filter.h"

#include "easy_atomic.h"
#include "easy_pool.h"
#include "easy_list.h"
#include "easy_hash.h"
#include "easy_log.h"

namespace taobao {

typedef struct tdhs_bloom_filter_t tdhs_bloom_filter_t;
struct tdhs_bloom_filter_t {
	bloom_filter* filter;
	easy_atomic_t filter_num;

	TDHS_INLINE
	void resest() {
		if (filter) {
			memset(filter->a, 0, filter->fsize);
		}
		easy_atomic_set(filter_num, 0);
	}

	TDHS_INLINE
	void destory() {
		if (filter) {
			destroy_bfilter(filter);
			filter = NULL;
		}
		easy_atomic_set(filter_num, 0);
	}

	TDHS_INLINE
	int check(uint64_t *hash) {
		if (filter) {
			return bfilter_check(filter, hash);
		}
		return 0;
	}

	TDHS_INLINE
	bool add_hash(uint64_t *hash) {
		if (filter && !bfilter_check(filter, hash)) {
			bfilter_add(filter, hash);
			easy_atomic_inc(&filter_num);
			return true;
		}
		return false;
	}

};

static tdhs_bloom_filter_t* filters = NULL;

static unsigned long long int current_filter_index = 0;

static bool filter_switch = false;

static easy_atomic_t switch_lock = 0;

int init_optimize() {
	if (tdhs_optimize_bloom_filter_group > 0
			&& tdhs_optimize_bloom_filter_num_buckets > 0) {
		filters =
				(tdhs_bloom_filter_t*) TAOBAO_MALLOC(sizeof(tdhs_bloom_filter_t)*tdhs_optimize_bloom_filter_group);
		if (filters == NULL) {
			easy_error_log(
					"TDHS: don't have enough memory for tdhs_bloom_filter_t");
			return EASY_ERROR;
		}
		memset(filters, 0,
				sizeof(tdhs_bloom_filter_t) * tdhs_optimize_bloom_filter_group);

		for (size_t i = 0; i < tdhs_optimize_bloom_filter_group; i++) {
			filters[i].filter = create_bfilter(
					tdhs_optimize_bloom_filter_num_buckets);
			if (filters[i].filter == NULL) {
				easy_error_log(
						"TDHS: don't have enough memory for bloom_filter");
				//free already alloced filter
				for (size_t j = 0; j < i; j++) {
					filters[j].destory();
				}

				TAOBAO_FREE(filters);
				filters = NULL;
				return EASY_ERROR;
			}
		}

	}
	return EASY_OK;
}

static TDHS_INLINE uint64_t calc_hash_table(const tdhs_request_table_t &table) {
	uint64_t hash = table.hash_code_table();
	return fold(hash, easy_hash_code(table.index.str, table.index.strlen(), 5));;
}

static TDHS_INLINE uint64_t calc_hash_key(const tdhs_get_key_t &get_key) {
	uint64_t hash = 0;
	for (uint32_t i = 0; i < get_key.key_field_num; i++) {
		if (hash) {
			hash = fold(hash,
					easy_hash_code(get_key.key[i].str, get_key.key[i].strlen(),
							5));
		} else {
			hash = easy_hash_code(get_key.key[i].str, get_key.key[i].strlen(),
					5);
		}
	}
	return hash;
}

static TDHS_INLINE uint64_t calc_hash_keys(uint32_t key_num,
		const tdhs_get_key_t* key) {
	uint64_t hash = 0;
	for (uint32_t i = 0; i < key_num; i++) {
		if (hash) {
			hash = fold(hash, calc_hash_key(key[i]));
		} else {
			hash = calc_hash_key(key[i]);
		}
	}
	return hash;
}

static TDHS_INLINE uint64_t calc_hash_get(const tdhs_request_t &req) {
	uint64_t hash = fold(calc_hash_table(req.table_info),
			calc_hash_keys(req.get.key_num, req.get.keys));
	hash = fold(hash, easy_hash_key((uint64_t) req.get.find_flag));
	hash = fold(hash, easy_hash_key((uint64_t) req.get.start));
	hash = fold(hash, easy_hash_key((uint64_t) req.get.limit));
	return hash;
}

static uint64_t poll_key = 0; //轮询用 平衡quick和slow用

tdhs_optimize_t optimize(const tdhs_request_t &request) {
	if (request.type == REQUEST_TYPE_GET
			|| request.type == REQUEST_TYPE_COUNT) {
		switch (thread_strategy) {
		case TDHS_THREAD_LV_1:
			return TDHS_QUICK;
			break;
		case TDHS_THREAD_LV_3:
#ifdef TDHS_ROW_CACHE
			if (tdhs_optimize_on) {
				return TDHS_QUICK;
				break;
			}
#else
			if (tdhs_optimize_on && filters) {
				uint64_t hash = calc_hash_get(request);
				request.optimize_hash = hash;
				tdhs_bloom_filter_t &filter = filters[current_filter_index
				% tdhs_optimize_bloom_filter_group];
				if (filter.check(&hash)) {
					optimize_lv3_assign_to_quick_count++;
					return TDHS_QUICK;
				} else {
					optimize_lv3_assign_to_slow_count++;
					return TDHS_SLOW;
				}
				break;
			}
#endif
			easy_info_log("TDHS: tdhs_optimize_on is off,use lv2");
		default:
			if (thread_strategy != TDHS_THREAD_LV_3) {
				easy_error_log(
						"TDHS: error thread_strategy [%d],default use lv2",
						thread_strategy);
			}
		case TDHS_THREAD_LV_2:
			uint64_t hash = easy_hash_key(poll_key++);
			if (hash % (tdhs_thread_num + active_slow_read_thread_num)
					<= tdhs_thread_num) {
				return TDHS_QUICK;
			} else {
				return TDHS_SLOW;
			}
			break;
		}
	} else { //MARK 针对request type 需要进行判断
		return TDHS_WRITE;
	}
	return TDHS_QUICK;
}

void add_optimize(const tdhs_request_t &request) {
#ifdef TDHS_ROW_CACHE
	if (1) {
		//enable row cache don't need add_optimize
		return;
	}
#endif
	if (tdhs_optimize_on && filters
			&& (request.type == REQUEST_TYPE_GET
					|| request.type == REQUEST_TYPE_COUNT)) {
		if (filter_switch) {
			//切换中..不计入统计
			return;
		}
		tdhs_bloom_filter_t &filter = filters[current_filter_index
				% tdhs_optimize_bloom_filter_group];
		if (filter.filter_num > tdhs_optimize_guess_hot_request_num) {
			if (easy_trylock(&switch_lock)) {
				filter_switch = true;
				current_filter_index++;
				filter.resest();
				filter_switch = false;
				easy_unlock(&switch_lock);
			} else {
				//锁失败,放弃
				return;
			}
		}

		uint64_t hash =
				request.optimize_hash ?
						request.optimize_hash : calc_hash_get(request);
		for (size_t i = 0; i < tdhs_optimize_bloom_filter_group; i++) {
			tdhs_bloom_filter_t &c_filter = filters[(current_filter_index + i)
					% tdhs_optimize_bloom_filter_group];
			if (!c_filter.add_hash(&hash)) {
				//filter中已有此hash 不再需要添加
				break;
			}
			if (c_filter.filter_num
					< (tdhs_optimize_guess_hot_request_num
							/ tdhs_optimize_bloom_filter_group)) {
				//此数据量太小,下一个filter还不需要放数据
				break;
			}

		}
	}
}

void destory_optimize() {
	if (filters) {
		for (size_t j = 0; j < tdhs_optimize_bloom_filter_group; j++) {
			filters[j].destory();
		}

		TAOBAO_FREE(filters);
		filters = NULL;
	}
}

void show_optimize_status(char* status, const size_t n) {
	memset(status, 0, n);
	if (tdhs_optimize_on && filters) {
		int len = 0;
		len += snprintf(status + len, n - len, "idx[%llu] num[",
				current_filter_index % tdhs_optimize_bloom_filter_group);
		for (size_t i = 0; i < tdhs_optimize_bloom_filter_group; i++) {
			len += snprintf(status + len, n - len, "%d,",
					filters[i].filter_num);
		}
		len--; //除去最后一个逗号
		snprintf(status + len, n - len, "]");
	} else {
		snprintf(status, n, "optimize is OFF");
	}
}

} // namespace taobao

