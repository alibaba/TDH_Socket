/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_monitor.cpp
 *
 *  Created on: 2011-11-8
 *      Author: wentong
 */

#include "tdh_socket_monitor.hpp"
#include "tdh_socket_thd.hpp"
#include "tdh_socket_statistic.hpp"
#include "thread_and_lock.hpp"
#include "util.hpp"
#include "tdh_socket_share.hpp"
#include "tdh_socket_config.hpp"

#include <string.h>

#include "easy_io.h"
#include "easy_log.h"

namespace taobao {

static pthread_t monitor_thread;

static int is_canceled = 0;

static void* monitor_thd_thread(void* p);

void start_monitor_thd() {
	pthread_attr_t attr;
	is_canceled = 0;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if (pthread_create(&monitor_thread, &attr, monitor_thd_thread, NULL) != 0) {
		easy_error_log("TDHS:pthread_create monitor error, error :%d (%s)\n",
				errno, strerror(errno));
		fatal_abort("pthread_create error");
	}
}

void cancel_monitor_thd() {
	easy_error_log("TDHS: cancel_monitor_thd");
	if (!is_canceled) {
		is_canceled = 1;
		pthread_join(monitor_thread, NULL);
		pthread_cancel(monitor_thread);
	}
}

class table_opener {
public:
	table_opener(THD* thd, const char *table_name) :
			_thd(thd) {
		bool refresh = true;
#if MYSQL_VERSION_ID >= 50505
		table_list.init_one_table(INFORMATION_SCHEMA_NAME.str,
				INFORMATION_SCHEMA_NAME.length, table_name, strlen(table_name),
				table_name, TL_READ);
		table_list.mdl_request.init(MDL_key::TABLE, INFORMATION_SCHEMA_NAME.str,
				table_name, MDL_SHARED_READ, MDL_TRANSACTION);
#else
		table_list.init_one_table(INFORMATION_SCHEMA_NAME.str, table_name,
				TL_READ);
#endif
		ST_SCHEMA_TABLE *schema_table = find_schema_table(thd,
				table_list.alias);
		if (schema_table) {
			table_list.schema_table = schema_table;
		} else {
			goto error_exit;
		}
		table_list.select_lex = &thd->lex->select_lex;
		if (mysql_schema_table(thd, thd->lex, &table_list)) {
			goto error_exit;
		}
		table_list.table->use_all_columns();
		table_list.set_table_ref_id(table_list.table->s);
		return;
		error_exit:

		table_list.table = NULL;
		easy_warn_log( "TDHS: failed to open status_table %p [%s] [%s] [%d]\n",
				thd, INFORMATION_SCHEMA_NAME.str, table_name, static_cast<int>(refresh));
		return;

	}

	~table_opener() {
		tb_assert(_thd!=NULL);
		close_thread_tables(_thd);
	}

	TABLE* get_table() {
		return table_list.table;
	}

	TABLE_LIST* get_table_list() {
		return &table_list;
	}
private:
	TABLE_LIST table_list;
	THD* _thd;
};

class table_locker {
public:
	table_locker(THD* thd, TABLE* status_table) :
			_thd(thd) {

#if MYSQL_VERSION_ID >= 50505
		lock = thd->lock = mysql_lock_tables(thd, &status_table, 0, 0);
#else
		bool need_reopen = false;
		lock = thd->lock = mysql_lock_tables(thd, &status_table, 0,
				MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN, &need_reopen);

#endif
		if (lock == NULL) {
			easy_error_log( "TDHS: lock status_table failed! thd [%p]", thd);
		}
	}
	~table_locker() {
		if (lock != NULL) {
			bool suc = true;
#if MYSQL_VERSION_ID >= 50505
			suc = (trans_commit_stmt(_thd) == 0);
#else
			suc = (ha_autocommit_or_rollback(_thd, 0) == 0);
#endif
			if (!suc) {
				easy_error_log( "TDHS: unlock status_table failed! thd [%p] ",
						_thd);
			}
			mysql_unlock_tables(_thd, lock);
			lock = _thd->lock = NULL;
		}
	}

	MYSQL_LOCK* get_lock() {
		return lock;
	}
private:
	THD* _thd;
	MYSQL_LOCK* lock;
};

typedef struct {
#define INNODB_BUFFER_POOL_READ_REQUESTS_NAME "innodb_buffer_pool_read_requests"
	long long int innodb_buffer_pool_read_requests;
#define INNODB_BUFFER_POOL_READS_NAME "innodb_buffer_pool_reads"
	long long int innodb_buffer_pool_reads;

	TDHS_INLINE
	bool is_done() {
		return innodb_buffer_pool_read_requests >= 0
				&& innodb_buffer_pool_reads >= 0;
	}

	TDHS_INLINE
	void reset() {
		innodb_buffer_pool_read_requests = -1;
		innodb_buffer_pool_reads = -1;
	}
} tdhs_innodb_status_t;

static TDHS_INLINE void check_innodb_status(tdhs_innodb_status_t &old_status,
		tdhs_innodb_status_t &new_status) {
	if (old_status.is_done() && new_status.is_done()) {
		long long int all_read = new_status.innodb_buffer_pool_read_requests
				- old_status.innodb_buffer_pool_read_requests;
		long long int io_read = new_status.innodb_buffer_pool_reads
				- old_status.innodb_buffer_pool_reads;

		if (all_read != 0) {
			long long int io_read_pre = io_read / tdhs_monitor_interval;
			last_io_read_per_second = io_read_pre;
			easy_debug_log(
					"TDHS:innodb use innodb_buffer_pool_read_requests/s lv1[%d] lv2[%d] now[%lli]",
					tdhs_thread_strategy_requests_lv_1, tdhs_thread_strategy_requests_lv_2, io_read_pre);

			if (io_read_pre < tdhs_thread_strategy_requests_lv_1) {
				thread_strategy = TDHS_THREAD_LV_1;
				active_slow_read_thread_num = 1;
			} else if (io_read_pre >= tdhs_thread_strategy_requests_lv_1
					&& io_read_pre <= tdhs_thread_strategy_requests_lv_2) {
				thread_strategy = TDHS_THREAD_LV_2;
				if (tdhs_thread_strategy_requests_lv_2
						<= tdhs_thread_strategy_requests_lv_1) {
					active_slow_read_thread_num = tdhs_slow_read_thread_num;
				} else {
					active_slow_read_thread_num = tdhs_slow_read_thread_num
							* (io_read_pre - tdhs_thread_strategy_requests_lv_1)
							/ (tdhs_thread_strategy_requests_lv_2
									- tdhs_thread_strategy_requests_lv_1);
					active_slow_read_thread_num =
							active_slow_read_thread_num > 0 ?
									active_slow_read_thread_num : 1;
				}

			} else if (io_read_pre > tdhs_thread_strategy_requests_lv_2) {
				thread_strategy = TDHS_THREAD_LV_3;
				active_slow_read_thread_num = tdhs_slow_read_thread_num;
			}

			easy_debug_log("TDHS:now thread_strategy [%d]", thread_strategy);
		} else {
			easy_debug_log("TDHS:innodb is sleeping~");
		}
	} else {
		easy_debug_log("TDHS:status is not done for check!");
	}

	old_status.innodb_buffer_pool_read_requests =
			new_status.innodb_buffer_pool_read_requests;
	old_status.innodb_buffer_pool_reads = new_status.innodb_buffer_pool_reads;
	new_status.reset();
}

static TDHS_INLINE void get_value_from_field(const char * status_name, size_t n,
		Field* const status_name_field, Field* const value_field,
		long long int *value) {
	char str_buf[64];
	String rwpstr(str_buf, sizeof(str_buf), &my_charset_bin);
	if (!status_name_field->is_null()) {
		status_name_field->val_str(&rwpstr, &rwpstr);
		if (strncasecmp(status_name, rwpstr.ptr(), n) == 0) {
			if (!value_field->is_null()) {
				*value = value_field->val_int();
				easy_debug_log("TDHS:get status %s [%lli]",
						status_name, *value);
			}
		}
	}
}

static void* monitor_thd_thread(void* p) {
	tdhs_innodb_status_t old_status = { -1, -1 };
	tdhs_innodb_status_t new_status = { -1, -1 };
	easy_error_log("TDHS: monitor_thd start");
	void *stack_bottom = NULL;
	char info[MAX_INFO_SIZE];
	THD* monitor_thd = init_THD(my_strdup("TDH_SOCKET_MONITOR", MYF(0)),
			&stack_bottom, false);
	if (monitor_thd == NULL) {
		easy_error_log("TDHS: monitor thd create failed!");
		return NULL;
	}
	wait_server_to_start(monitor_thd);
	monitor_thd->lex->wild = NULL; //show status会用到.不然会crash
	lex_start(monitor_thd);
	unsigned int skip = 0;
	while (!tdhs_share->shutdown && !is_canceled) {
		thd_proc_info(monitor_thd, info);
		set_thread_message(info, "TDHS:ZZZzzz...");
		sleep(1);
		set_thread_message(info, "TDHS:I'm working~");
		{ //check if killed
			tdhs_mysql_mutex_lock(&monitor_thd->mysys_var->mutex);
			THD::killed_state st = monitor_thd->killed;
			tdhs_mysql_mutex_unlock(&monitor_thd->mysys_var->mutex);
			easy_debug_log("TDHS:check monitor THD status [%d]", st);
			if (st != THD::NOT_KILLED) {
				easy_error_log(
						"TDHS: monitor thd st is %d ,should be stop eio!", st);
				tdhs_close_cached_table();
				if (!easy_io_stop()) {
					easy_io_wait();
					easy_io_destroy();
					tdhs_share->shutdown = 1;
					break;
				}
			}
		}
		if (++skip < tdhs_monitor_interval) {
			continue;
		}
		skip = 0;
		{
			table_opener _opener(monitor_thd, "GLOBAL_STATUS");
			TABLE* status_table = _opener.get_table();
			if (status_table == NULL) {
				continue;
			}
			table_locker _locker(monitor_thd, status_table);
			MYSQL_LOCK* lock = _locker.get_lock();
			if (lock == NULL) {
				continue;
			}

			_opener.get_table_list()->schema_table->fill_table(monitor_thd,
					_opener.get_table_list(), NULL);

			status_table->read_set = &status_table->s->all_set;
			handler * const hnd = status_table->file;
			hnd->init_table_handle_for_HANDLER();
			hnd->ha_index_or_rnd_end();
			hnd->ha_rnd_init(true);
			int r = 0;
			Field* const status_name_field = status_table->field[0];
			Field* const value_field = status_table->field[1];
			if (status_name_field == NULL || value_field == NULL) {
				easy_error_log("TDHS:can't find status_name and value field");
				continue;
			}
			while (true) {
				r = hnd->rnd_next(status_table->record[0]);
				if (r != 0 && r != HA_ERR_RECORD_DELETED) {

					easy_debug_log("TDHS:rnd status need break [%d]", r);
					//something error
					if (r != HA_ERR_END_OF_FILE) {
						easy_error_log("TDHS:rnd status error [%d]", r);
					}
					break;
				}
				get_value_from_field(INNODB_BUFFER_POOL_READ_REQUESTS_NAME,
						sizeof(INNODB_BUFFER_POOL_READ_REQUESTS_NAME),
						status_name_field, value_field,
						&new_status.innodb_buffer_pool_read_requests);
				get_value_from_field(INNODB_BUFFER_POOL_READS_NAME,
						sizeof(INNODB_BUFFER_POOL_READS_NAME),
						status_name_field, value_field,
						&new_status.innodb_buffer_pool_reads);
				if (new_status.is_done()) {
					easy_debug_log("TDHS:innodb status get done")
					break;
				}
			}
			hnd->ha_index_or_rnd_end();
			check_innodb_status(old_status, new_status);
		}

	}
	destory_thd(monitor_thd);
	easy_error_log("TDHS: monitor_thd end");
	return NULL;
}

} // namespace taobao

