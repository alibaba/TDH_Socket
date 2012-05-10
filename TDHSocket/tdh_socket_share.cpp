/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_share.cpp
 *
 *  Created on: 2011-10-11
 *      Author: wentong
 */

#include "tdh_socket_share.hpp"
#include "tdh_socket_config.hpp"
#include "thread_and_lock.hpp"
#include "tdh_socket_request_thread.hpp"
#include "tdh_socket_time.hpp"
#include "tdh_socket_statistic.hpp"
#include <stddef.h>

namespace taobao {

easy_thread_pool_t *request_server_tp;

easy_thread_pool_t *slow_read_request_server_tp;

easy_thread_pool_t *write_request_server_tp;

static tdhs_share_t _tdhs_share;

tdhs_share_t* tdhs_share = &_tdhs_share;

tdhs_thread_strategy_t thread_strategy = TDHS_THREAD_LV_2; //默认一开始LV2

unsigned int active_slow_read_thread_num = 1;

//auth

#define MAX_AUTH_CODE_LENGTH 256

static pthread_rwlock_t auth_rw_lock;

static char tdhs_auth_read_code_char[MAX_AUTH_CODE_LENGTH];

static char tdhs_auth_write_code_char[MAX_AUTH_CODE_LENGTH];

static tdhs_string_t _tdhs_auth_read_code_string =
		{ tdhs_auth_read_code_char, 0 };

static tdhs_string_t _tdhs_auth_write_code_string = { tdhs_auth_write_code_char,
		0 };

tdhs_string_t* tdhs_auth_read_code_string = &_tdhs_auth_read_code_string;

tdhs_string_t* tdhs_auth_write_code_string = &_tdhs_auth_write_code_string;

int init_auth() {
	if (pthread_rwlock_init(&auth_rw_lock, NULL) != 0) {
		return EASY_ERROR;
	}
	reset_auth_read_code();
	reset_auth_write_code();
	return EASY_OK;
}

int destory_auth() {
	if (pthread_rwlock_destroy(&auth_rw_lock) != 0) {
		return EASY_ERROR;
	}
	return EASY_OK;
}

bool tdhs_auth_read(tdhs_string_t &str) {
	bool ret;
	pthread_rwlock_rdlock(&auth_rw_lock);
	ret = tdhs_auth_read_code_string->compare(str) == 0;
	pthread_rwlock_unlock(&auth_rw_lock);
	return ret;
}

bool tdhs_auth_write(tdhs_string_t &str) {
	bool ret;
	pthread_rwlock_rdlock(&auth_rw_lock);
	ret = tdhs_auth_write_code_string->compare(str) == 0;
	pthread_rwlock_unlock(&auth_rw_lock);
	return ret;
}

void reset_auth_read_code() {
	pthread_rwlock_wrlock(&auth_rw_lock);
	if (tdhs_auth_read_code) {
		size_t code_len = strlen(tdhs_auth_read_code);
		code_len =
				code_len >= MAX_AUTH_CODE_LENGTH ?
						MAX_AUTH_CODE_LENGTH - 1 : code_len;
		tdhs_auth_read_code_string->len = code_len + 1;
		memset(tdhs_auth_read_code_char, 0, MAX_AUTH_CODE_LENGTH);
		memcpy(tdhs_auth_read_code_char, tdhs_auth_read_code, code_len);
	} else {
		tdhs_auth_read_code_string->len = 0;
		memset(tdhs_auth_read_code_char, 0, MAX_AUTH_CODE_LENGTH);
	}
	tdhs_auth_read_code = tdhs_auth_read_code_char;
	pthread_rwlock_unlock(&auth_rw_lock);
}

void reset_auth_write_code() {
	pthread_rwlock_wrlock(&auth_rw_lock);
	if (tdhs_auth_write_code) {
		size_t code_len = strlen(tdhs_auth_write_code);
		code_len =
				code_len >= MAX_AUTH_CODE_LENGTH ?
						MAX_AUTH_CODE_LENGTH - 1 : code_len;
		tdhs_auth_write_code_string->len = code_len + 1;
		memset(tdhs_auth_write_code_char, 0, MAX_AUTH_CODE_LENGTH);
		memcpy(tdhs_auth_write_code_char, tdhs_auth_write_code, code_len);
	} else {
		tdhs_auth_write_code_string->len = 0;
		memset(tdhs_auth_write_code_char, 0, MAX_AUTH_CODE_LENGTH);
	}
	tdhs_auth_write_code = tdhs_auth_write_code_char;
	pthread_rwlock_unlock(&auth_rw_lock);
}

void tdhs_close_cached_table() {
	taobao::tdhs_cache_table_on = 0;
	//notify all request thread for close cached table
	//MARK 有新的request thread pool也需要在这里被wakeup
	taobao::wakeup_request_thread(taobao::request_server_tp);
	taobao::wakeup_request_thread(taobao::slow_read_request_server_tp);
	taobao::wakeup_request_thread(taobao::write_request_server_tp);
}

//throttle

ullong slow_read_io_num = 0;

#define NANOSECONDS_PER_SECOND (1000000)
#define SLOW_READ_SLICE_TIME  (1*NANOSECONDS_PER_SECOND/10)

typedef struct {
	ullong start_read_io_num;
	ullong slice_start; //us
	ullong slice_end; //us
	ullong slice_time; //us
} tdhs_throttle_t;

static tdhs_throttle_t slow_read_throttle = { 0, 0, 0, 0 };

bool slow_read_limit() {
	ullong now, elapsed_time, slice_time, wait_time;
	double io_limits, ios_limit, ios_base;
	if (!tdhs_throttle_on) {
		return false;
	}
	io_limits = tdhs_slow_read_limits;
	now = tdhs_micro_time();
	if (slow_read_throttle.slice_start < now
			&& slow_read_throttle.slice_end > now) {
		slow_read_throttle.slice_end = now + slow_read_throttle.slice_time;
	} else {
		slow_read_throttle.slice_time = 5 * SLOW_READ_SLICE_TIME;
		slow_read_throttle.slice_start = now;
		slow_read_throttle.slice_end = now + slow_read_throttle.slice_time;
		slow_read_throttle.start_read_io_num = slow_read_io_num;
	}

	elapsed_time = now - slow_read_throttle.slice_start;

	slice_time = slow_read_throttle.slice_end - slow_read_throttle.slice_start;
	ios_limit = io_limits / (slice_time / (double) NANOSECONDS_PER_SECOND);
	ios_limit = ios_limit > 0 ? ios_limit : 1; //防止可能的配置导致的0
	ios_base = slow_read_io_num - slow_read_throttle.start_read_io_num;
	if (ios_base < ios_limit) {
		//access
		slow_read_io_num++;
		return false;
	} else {
		throttle_count++;
	}
	/* Calc approx time to dispatch */
	wait_time = (ios_base + 1) / ios_limit * NANOSECONDS_PER_SECOND;
	wait_time = (wait_time > elapsed_time) ? (wait_time - elapsed_time) : 0;

	slow_read_throttle.slice_time = wait_time * SLOW_READ_SLICE_TIME * 10;
	slow_read_throttle.slice_end += slow_read_throttle.slice_time
			- 3 * SLOW_READ_SLICE_TIME;

	return true;
}

} // namespace taobao

