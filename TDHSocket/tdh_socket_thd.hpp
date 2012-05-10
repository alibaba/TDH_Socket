/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_thd.hpp
 *
 *  Created on: 2011-12-28
 *      Author: wentong
 */

#ifndef TDH_SOCKET_THD_HPP_
#define TDH_SOCKET_THD_HPP_

#include "tdh_socket_share.hpp"
#include "debug_util.hpp"
#include "mysql_inc.hpp"
#include "easy_log.h"

namespace taobao {

#define MAX_INFO_SIZE 256

static TDHS_INLINE int set_thread_message(char* info, const char *fmt, ...);

static TDHS_INLINE THD* init_THD(char* db, const void *stack_bottom,
		bool need_write);

static TDHS_INLINE void destory_thd(THD *thd);

static TDHS_INLINE int wait_server_to_start(THD *thd);

static TDHS_INLINE int set_thread_message(char* info, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	const int n = vsnprintf(info, MAX_INFO_SIZE, fmt, ap);
	va_end(ap);
	return n;
}

static TDHS_INLINE THD* init_THD(char* db, const void *stack_bottom,
		bool need_write) {
	THD *thd = NULL;
	my_thread_init();
	thd = new THD;
	if (thd == NULL) {
		my_thread_end();
		return NULL;
	}
	thd->thread_stack = (char*) stack_bottom;
	easy_debug_log("TDHS:thread_stack = %p sizeof(THD)=%zu sizeof(mtx)=%zu ",
			thd->thread_stack, sizeof(THD), sizeof(LOCK_thread_count));
	thd->store_globals();
	thd->system_thread = static_cast<enum_thread_type>(1 << 30UL);
	const NET v = { 0 };
	thd->net = v;
	if (need_write) {
		//for write
#if MYSQL_VERSION_ID >= 50505
		thd->variables.option_bits |= OPTION_BIN_LOG;
#else
		thd->options |= OPTION_BIN_LOG;
#endif
	}
	//for db
	safeFree(thd->db);
	thd->db = db;
	my_pthread_setspecific_ptr(THR_THD, thd);

	tdhs_mysql_mutex_lock(&LOCK_thread_count);
	thd->thread_id = thread_id++;
	threads.append(thd);
	++thread_count;
	tdhs_mysql_mutex_unlock(&LOCK_thread_count);
	return thd;
}

static TDHS_INLINE void destory_thd(THD *thd) {
	tb_assert(thd!=NULL);
	my_pthread_setspecific_ptr(THR_THD, 0);

	tdhs_mysql_mutex_lock(&LOCK_thread_count);
	delete thd;
	--thread_count;
	tdhs_mysql_mutex_unlock(&LOCK_thread_count);
	my_thread_end();
	(void) tdhs_mysql_cond_broadcast(&COND_thread_count);
}

static TDHS_INLINE int wait_server_to_start(THD *thd) {
	int r = 0;
	tdhs_mysql_mutex_lock(&LOCK_server_started);
	while (!mysqld_server_started) {
		timespec abstime = { };
		set_timespec(abstime, 1);
		tdhs_mysql_cond_timedwait(&COND_server_started, &LOCK_server_started,
				&abstime);
		tdhs_mysql_mutex_unlock(&LOCK_server_started);
		tdhs_mysql_mutex_lock(&thd->mysys_var->mutex);
		THD::killed_state st = thd->killed;
		tdhs_mysql_mutex_unlock(&thd->mysys_var->mutex);
		tdhs_mysql_mutex_lock(&LOCK_server_started);
		if (st != THD::NOT_KILLED) {
			r = -1;
			break;
		}
		if (tdhs_share->shutdown) {
			r = -1;
			break;
		}
	}

	tdhs_mysql_mutex_unlock(&LOCK_server_started);
	return r;
}

} // namespace taobao

#endif /* TDH_SOCKET_THD_HPP_ */
