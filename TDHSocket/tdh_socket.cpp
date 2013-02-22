/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 ============================================================================
 Name        : tdh_socket.cpp
 Author      : wentong@taobao.com
 Version     :
 ============================================================================
 */
#include "tdh_socket_config.hpp"
#include "debug_util.hpp"
#include "tdh_socket_protocol.hpp"
#include "tdh_socket_handler.hpp"
#include "tdh_socket_share.hpp"
#include "tdh_socket_monitor.hpp"
#include "tdh_socket_define.hpp"
#include "tdh_socket_optimize.hpp"
#include "tdh_socket_statistic.hpp"
#include "tdh_socket_request_thread.hpp"
#include "default_mysql_sysvar_for_tdh_socket.hpp"

#include "mysql_inc.hpp"
#include <stdlib.h>
#include <ctype.h>
#include <mysql/plugin.h>
#include <my_global.h>
#include <my_dir.h>

#include <easy_log.h>
#include <easy_io.h>
#include <easy_define.h>
#include <easy_request.h>

/*
 Disable __attribute__() on non-gcc compilers.
 */
#if !defined(__attribute__) && !defined(__GNUC__)
#define __attribute__(A)
#endif

void thds_log_print_default(const char *message) {
	fprintf(stderr, "%s", message);
}

static easy_io_handler_pt io_handler;

/*
 Initialize the tdh socket at server start or plugin installation.

 SYNOPSIS
 tdh_socket_plugin_init()

 DESCRIPTION
 Starts up lister thread

 RETURN VALUE
 0                    success
 1                    failure
 */

static int tdh_socket_plugin_init(void *p) {
	easy_listen_t *listen;
	DBUG_ENTER("tdh_socket_plugin_init");
	easy_log_level = (easy_log_level_t) taobao::tdhs_log_level;
	easy_log_set_print(thds_log_print_default);

	if (!easy_io_create(taobao::tdhs_io_thread_num)) {
		easy_error_log("TDHS:easy_io_init error.\n");
		DBUG_RETURN(1);
	}

	taobao::request_server_tp = taobao::tdhs_thread_pool_create_ex(&easy_io_var,
			taobao::tdhs_thread_num, taobao::request_thread_start,
			taobao::on_server_process, NULL);

	if (taobao::request_server_tp == NULL) {
		easy_error_log("TDHS:easy_thread_pool_create_for_handler error.\n");
		DBUG_RETURN(1);
	}

	taobao::slow_read_request_server_tp = taobao::tdhs_thread_pool_create_ex(
			&easy_io_var, taobao::tdhs_slow_read_thread_num,
			taobao::slow_request_thread_start, taobao::on_server_process, NULL);

	if (taobao::slow_read_request_server_tp == NULL) {
		easy_error_log("TDHS:easy_thread_pool_create_for_handler error.\n");
		DBUG_RETURN(1);
	}

	taobao::write_request_server_tp = taobao::tdhs_thread_pool_create_ex(
			&easy_io_var, taobao::tdhs_write_thread_num,
			taobao::write_request_thread_start, taobao::on_server_process,
			NULL);

	if (taobao::write_request_server_tp == NULL) {
		easy_error_log("TDHS:easy_thread_pool_create_for_handler error.\n");
		DBUG_RETURN(1);
	}

	memset(&io_handler, 0, sizeof(io_handler));
	io_handler.decode = taobao::tdhs_decode;
	io_handler.encode = taobao::tdhs_encode;
	io_handler.on_connect = taobao::on_server_connect;
	io_handler.on_disconnect = taobao::on_server_disconnect;
	io_handler.process = taobao::on_server_io_process;
	io_handler.cleanup = taobao::cleanup_request;
//	io_handler.is_uthread = 1;

	if ((listen =
			easy_io_add_listen(NULL, taobao::tdhs_listen_port, &io_handler))
			== NULL) {
		easy_error_log("TDHS:easy_io_add_listen error, port: %d, %s\n",
				taobao::tdhs_listen_port, strerror(errno));
		DBUG_RETURN(1);
	} else {
		easy_error_log("TDHS:tdh_socket listen start, port = %d\n",
				taobao::tdhs_listen_port);
	}

	if (easy_io_start()) {
		easy_error_log("TDHS:easy_io_start error.\n");
		DBUG_RETURN(1);
	}
	taobao::tdhs_share->shutdown = 0;
	if (taobao::init_auth() != EASY_OK || taobao::init_optimize() != EASY_OK) {
		if (!easy_io_stop()) {
			easy_io_wait();
			easy_io_destroy();
			taobao::tdhs_share->shutdown = 1;
		}
		DBUG_RETURN(1);
	}
	taobao::start_monitor_thd();
	DBUG_RETURN(0);
}

/*
 Terminate the tdh socket at server shutdown or plugin deinstallation.

 SYNOPSIS
 tdh_socket_plugin_deinit()
 Does nothing.

 RETURN VALUE
 0                    success
 1                    failure

 */

static int tdh_socket_plugin_deinit(void *p) {
	DBUG_ENTER("tdh_socket_plugin_deinit");
	easy_error_log("TDHS:tdh_socket unload,shutdown is %d !",
			taobao::tdhs_share->shutdown);
	taobao::cancel_monitor_thd();
	taobao::destory_auth();
	taobao::destory_optimize();
	if (!taobao::tdhs_share->shutdown) {
		taobao::tdhs_close_cached_table();
		if (!easy_io_stop()) {
			easy_io_wait();
			easy_io_destroy();
			taobao::tdhs_share->shutdown = 1;
			DBUG_RETURN(0);
		} else {
			DBUG_RETURN(1);
		}
	}
	DBUG_RETURN(0);
}

static
void thds_log_level_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_log_level = *static_cast<const uint*>(save);
	easy_log_level = (easy_log_level_t) taobao::tdhs_log_level;

}

static
void thds_monitor_interval_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_monitor_interval = *static_cast<const uint*>(save);
}

static
void tdhs_thread_strategy_requests_lv_1_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_thread_strategy_requests_lv_1 =
			*static_cast<const uint*>(save);
}

static
void tdhs_thread_strategy_requests_lv_2_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_thread_strategy_requests_lv_2 =
			*static_cast<const uint*>(save);
}

static
void tdhs_optimize_on_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	if (*(my_bool*) save) {
		taobao::tdhs_optimize_on = 1;
	} else {
		taobao::tdhs_optimize_on = 0;
	}
}

static
void thds_optimize_guess_hot_request_num_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_optimize_guess_hot_request_num =
			*static_cast<const int*>(save);
}

static
void tdhs_cache_table_on_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	if (*(my_bool*) save) {
		taobao::tdhs_cache_table_on = 1;
	} else {
		taobao::tdhs_close_cached_table();
	}
}

static
void tdhs_concurrency_insert_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	if (*(my_bool*) save) {
		taobao::tdhs_concurrency_insert = 1;
	} else {
		taobao::tdhs_concurrency_insert = 0;
	}
}

static
void tdhs_concurrency_update_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	if (*(my_bool*) save) {
		taobao::tdhs_concurrency_update = 1;
	} else {
		taobao::tdhs_concurrency_update = 0;
	}
}

static
void tdhs_concurrency_delete_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	if (*(my_bool*) save) {
		taobao::tdhs_concurrency_delete = 1;
	} else {
		taobao::tdhs_concurrency_delete = 0;
	}
}

static
void tdhs_auth_on_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	if (*(my_bool*) save) {
		taobao::tdhs_auth_on = 1;
	} else {
		taobao::tdhs_auth_on = 0;
	}
}

static
void tdhs_auth_read_code_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	*static_cast<const char**>(var_ptr) =
			*static_cast<const char* const *>(save);
	taobao::reset_auth_read_code();
}

static
void tdhs_auth_write_code_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	*static_cast<const char**>(var_ptr) =
			*static_cast<const char* const *>(save);
	taobao::reset_auth_write_code();
}

static
void tdhs_throttle_on_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	if (*(my_bool*) save) {
		taobao::tdhs_throttle_on = 1;
	} else {
		taobao::tdhs_throttle_on = 0;
	}
}

static
void tdhs_slow_read_limits_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_slow_read_limits = *static_cast<const uint*>(save);
}

static
void tdhs_quick_request_thread_task_count_limit_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_quick_request_thread_task_count_limit =
			*static_cast<const uint*>(save);
}

static
void tdhs_slow_request_thread_task_count_limit_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_slow_request_thread_task_count_limit =
			*static_cast<const uint*>(save);
}

static
void tdhs_write_request_thread_task_count_limit_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_write_request_thread_task_count_limit =
			*static_cast<const uint*>(save);
}

static
void tdhs_group_commit_update(
/*===========================*/
THD* thd, /*!< in: thread handle */
struct st_mysql_sys_var* var, /*!< in: pointer to
 system variable */
void* var_ptr, /*!< out: where the
 formal string goes */
const void* save) /*!< in: immediate result
 from check function */
{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);

	if (*(my_bool*) save) {
		taobao::tdhs_group_commit = 1;
	} else {
		taobao::tdhs_group_commit = 0;
	}
}

static
void tdhs_group_commit_limit_update(THD* thd, //in: thread handle
		struct st_mysql_sys_var* var, // in: pointer to system variable
		void* var_ptr, // out: where the formal string goes
		const void* save) // in: immediate result from check function
		{
	tb_assert(var_ptr != NULL);tb_assert(save != NULL);
	taobao::tdhs_group_commit_limits =
			*static_cast<const int*>(save);
}

static MYSQL_SYSVAR_INT(listen_port, taobao::tdhs_listen_port, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"tdh socket listen port", NULL,NULL, DEFAULT_TDHS_LISTEN_PORT, 1024, 65535, 0);

static MYSQL_SYSVAR_UINT(log_level, taobao::tdhs_log_level, PLUGIN_VAR_RQCMDARG,
		"tdhs log level  OFF:1 FATAL:2 ERROR:3 WARN:4 INFO:5 DEBUG:6 TRACE:7 ALL:8", NULL,
		thds_log_level_update, DEFAULT_TDHS_LOG_LEVEL, 1, 8, 0);

static MYSQL_SYSVAR_UINT(io_thread_num, taobao::tdhs_io_thread_num, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"tdh socket io thread number", NULL,NULL, DEFAULT_TDHS_IO_THREAD_NUM, 1, 64, 0);

static MYSQL_SYSVAR_UINT(thread_num, taobao::tdhs_thread_num, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"tdh socket thread number", NULL,NULL, DEFAULT_TDHS_THREAD_NUM, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(slow_read_thread_num, taobao::tdhs_slow_read_thread_num, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"tdh socket thread number for slow read", NULL,NULL, DEFAULT_TDHS_SLOW_READ_THREAD_NUM, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(write_thread_num, taobao::tdhs_write_thread_num, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"tdh socket thread number for write", NULL,NULL, DEFAULT_TDHS_WRITE_THREAD_NUM, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_BOOL(cache_table_on, taobao::tdhs_cache_table_on,
		PLUGIN_VAR_NOCMDARG,
		"switch cache_table ,when cache_table is off,it will close all cached tables!",
		NULL, tdhs_cache_table_on_update, DEFAULT_TDHS_CACHE_TABLE_ON);

static MYSQL_SYSVAR_UINT(cache_table_num_for_thd, taobao::tdhs_cache_table_num_for_thd, PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"tdh socket setting the cache table number for per thread", NULL,NULL, DEFAULT_TDHS_CACHE_TABLE_NUM_FOR_THD, 1, 255, 0);

static MYSQL_SYSVAR_BOOL(optimize_on, taobao::tdhs_optimize_on,
		PLUGIN_VAR_NOCMDARG,
		"switch optimize!",
		NULL, tdhs_optimize_on_update, DEFAULT_TDHS_OPTIMIZE_ON);

static MYSQL_SYSVAR_UINT(optimize_bloom_filter_group, taobao::tdhs_optimize_bloom_filter_group,
		PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"tdh socket optimize bloom filter group number(how many bloom filter work.)", NULL,NULL, DEFAULT_TDHS_OPTIMIZE_BLOOM_FILTER_GROUP, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(optimize_bloom_filter_num_buckets, taobao::tdhs_optimize_bloom_filter_num_buckets,
		PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"tdh socket optimize bloom filter buckets num", NULL,NULL, DEFAULT_TDHS_OPTIMIZE_BLOOM_FILTER_NUM_BUCKETS, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_INT(optimize_guess_hot_request_num, taobao::tdhs_optimize_guess_hot_request_num,
		PLUGIN_VAR_RQCMDARG ,
		"tdh socket optimize guess hot request num", NULL,thds_optimize_guess_hot_request_num_update, DEFAULT_TDHS_OPTIMIZE_GUESS_HOT_REQUEST_NUM, 1, INT_MAX, 0);

static MYSQL_SYSVAR_UINT(monitor_interval, taobao::tdhs_monitor_interval, PLUGIN_VAR_RQCMDARG,
		"tdhs monitor interval (second) , for calc cache hit radio time interval", NULL,
		thds_monitor_interval_update, DEFAULT_TDHS_MONITOR_INTERVAL, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(thread_strategy_requests_lv_1, taobao::tdhs_thread_strategy_requests_lv_1,
		PLUGIN_VAR_RQCMDARG,
		"tdhs thread_strategy_requests_lv_1 which innodb's Innodb_buffer_pool_read_requests ", NULL,
		tdhs_thread_strategy_requests_lv_1_update, DEFAULT_TDHS_THREAD_STRATEGY_REQUESTS_LV_1, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(thread_strategy_requests_lv_2, taobao::tdhs_thread_strategy_requests_lv_2,
		PLUGIN_VAR_RQCMDARG,
		"tdhs thread_strategy_requests_lv_2 which innodb's Innodb_buffer_pool_read_requests", NULL,
		tdhs_thread_strategy_requests_lv_2_update, DEFAULT_TDHS_THREAD_STRATEGY_REQUESTS_LV_2, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_BOOL(concurrency_insert, taobao::tdhs_concurrency_insert,
		PLUGIN_VAR_NOCMDARG,
		"switch concurrency_insert!",
		NULL, tdhs_concurrency_insert_update, DEFAULT_TDHS_CONCURRENCY_INSERT);

static MYSQL_SYSVAR_BOOL(concurrency_update, taobao::tdhs_concurrency_update,
		PLUGIN_VAR_NOCMDARG,
		"switch tdhs_concurrency_update!",
		NULL, tdhs_concurrency_update_update, DEFAULT_TDHS_CONCURRENCY_UPDATE);

static MYSQL_SYSVAR_BOOL(concurrency_delete, taobao::tdhs_concurrency_delete,
		PLUGIN_VAR_NOCMDARG,
		"switch concurrency_delete!",
		NULL, tdhs_concurrency_delete_update, DEFAULT_TDHS_CONCURRENCY_DELETE);

static MYSQL_SYSVAR_BOOL(auth_on, taobao::tdhs_auth_on,
		PLUGIN_VAR_NOCMDARG,
		"switch need auth!",
		NULL, tdhs_auth_on_update, DEFAULT_TDHS_AUTH_ON);

static MYSQL_SYSVAR_STR(auth_read_code, taobao::tdhs_auth_read_code,
		PLUGIN_VAR_OPCMDARG,
		"The read code.",
		NULL,
		tdhs_auth_read_code_update, DEFAULT_TDHS_AUTH_READ_CODE);

static MYSQL_SYSVAR_STR(auth_write_code, taobao::tdhs_auth_write_code,
		PLUGIN_VAR_OPCMDARG,
		"The write code.",
		NULL,
		tdhs_auth_write_code_update, DEFAULT_TDHS_AUTH_WRITE_CODE);

static MYSQL_SYSVAR_BOOL(throttle_on, taobao::tdhs_throttle_on,
		PLUGIN_VAR_NOCMDARG,
		"switch need throttle!",
		NULL, tdhs_throttle_on_update, DEFAULT_TDHS_THROTTLE_ON);

static MYSQL_SYSVAR_UINT(slow_read_limits, taobao::tdhs_slow_read_limits,
		PLUGIN_VAR_RQCMDARG,
		"for throttle,limit the slow read number ", NULL,
		tdhs_slow_read_limits_update, DEFAULT_TDHS_SLOW_READ_LIMITS, 1, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(write_buff_size, taobao::tdhs_write_buff_size,
		PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
		"the write buffer size", NULL,
		NULL, DEFAULT_TDHS_WRITE_BUFF_SIZE, 1*1024, UINT_MAX, 0);

static MYSQL_SYSVAR_BOOL(group_commit, taobao::tdhs_group_commit,
		PLUGIN_VAR_NOCMDARG,
		"switch group commit!",
		NULL, tdhs_group_commit_update, DEFAULT_TDHS_GROUP_COMMIT);

static MYSQL_SYSVAR_INT(group_commit_limits, taobao::tdhs_group_commit_limits,
		PLUGIN_VAR_RQCMDARG,
		"the limit of the group commit max number , 0 mean unlimited", NULL,
		tdhs_group_commit_limit_update, DEFAULT_TDHS_GROUP_COMMIT_LIMITS,0, INT_MAX, 0);

static MYSQL_SYSVAR_UINT(quick_request_thread_task_count_limit, taobao::tdhs_quick_request_thread_task_count_limit,
		PLUGIN_VAR_RQCMDARG,
		"the limit of the quick request thread's max task count , 0 mean unlimited", NULL,
		tdhs_quick_request_thread_task_count_limit_update, DEFAULT_TDHS_QUICK_REQUEST_THREAD_TASK_COUNT_LIMIT,0, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(slow_request_thread_task_count_limit, taobao::tdhs_slow_request_thread_task_count_limit,
		PLUGIN_VAR_RQCMDARG,
		"the limit of the slow request thread's max task count , 0 mean unlimited", NULL,
		tdhs_slow_request_thread_task_count_limit_update, DEFAULT_TDHS_SLOW_REQUEST_THREAD_TASK_COUNT_LIMIT,0, UINT_MAX, 0);

static MYSQL_SYSVAR_UINT(write_request_thread_task_count_limit, taobao::tdhs_write_request_thread_task_count_limit,
		PLUGIN_VAR_RQCMDARG,
		"the limit of the write request thread's max task count , 0 mean unlimited", NULL,
		tdhs_write_request_thread_task_count_limit_update, DEFAULT_TDHS_WRITE_REQUEST_THREAD_TASK_COUNT_LIMIT,0, UINT_MAX, 0);

/* warning: type-punning to incomplete type might break strict-aliasing
 * rules */
static struct st_mysql_sys_var *daemon_tdh_socket_system_variables[] = {
		MYSQL_SYSVAR(listen_port), MYSQL_SYSVAR(log_level), MYSQL_SYSVAR(
				io_thread_num), MYSQL_SYSVAR(slow_read_thread_num),
		MYSQL_SYSVAR(write_thread_num), MYSQL_SYSVAR(thread_num), MYSQL_SYSVAR(
				cache_table_on), MYSQL_SYSVAR(cache_table_num_for_thd),
		MYSQL_SYSVAR(optimize_on), MYSQL_SYSVAR(optimize_bloom_filter_group),
		MYSQL_SYSVAR(optimize_bloom_filter_num_buckets), MYSQL_SYSVAR(
				optimize_guess_hot_request_num), MYSQL_SYSVAR(monitor_interval),
		MYSQL_SYSVAR(thread_strategy_requests_lv_1), MYSQL_SYSVAR(
				thread_strategy_requests_lv_2), MYSQL_SYSVAR(
				concurrency_insert), MYSQL_SYSVAR(concurrency_update),
		MYSQL_SYSVAR(concurrency_delete), MYSQL_SYSVAR(auth_on), MYSQL_SYSVAR(
				auth_read_code), MYSQL_SYSVAR(auth_write_code), MYSQL_SYSVAR(
				throttle_on), MYSQL_SYSVAR(slow_read_limits), MYSQL_SYSVAR(
				write_buff_size), MYSQL_SYSVAR(group_commit),
		MYSQL_SYSVAR(group_commit_limits),
		MYSQL_SYSVAR(quick_request_thread_task_count_limit),
		MYSQL_SYSVAR(slow_request_thread_task_count_limit),
		MYSQL_SYSVAR(write_request_thread_task_count_limit), 0 };

static char _optimize_status[1024];

static char *optimize_status = _optimize_status;

static char _io_status[1024];

static char *io_status = _io_status;

static SHOW_VAR hs_status_variables[] = { { "table_open",
		(char*) &taobao::open_tables_count, SHOW_LONGLONG }, //
		{ "table_close", (char*) &taobao::close_tables_count, SHOW_LONGLONG }, //
		{ "table_lock", (char*) &taobao::lock_tables_count, SHOW_LONGLONG }, //
		{ "table_unlock", (char*) &taobao::unlock_tables_count, SHOW_LONGLONG }, //
		{ "get_count", (char*) &taobao::get_count, SHOW_LONGLONG }, //
		{ "count_count", (char*) &taobao::count_count, SHOW_LONGLONG }, //
		{ "update_count", (char*) &taobao::update_count, SHOW_LONGLONG }, //
		{ "delete_count", (char*) &taobao::delete_count, SHOW_LONGLONG }, //
		{ "insert_count", (char*) &taobao::insert_count, SHOW_LONGLONG }, //
		{ "batch_count", (char*) &taobao::batch_count, SHOW_LONGLONG }, //
		{ "thread_strategy", (char*) &taobao::thread_strategy, SHOW_INT }, //
		{ "last_io_read_per_second", (char*) &taobao::last_io_read_per_second,
				SHOW_LONGLONG }, //
		{ "optimize_status", (char*) &optimize_status, SHOW_CHAR_PTR }, //
		{ "optimize_lv3_assign_to_quick_count",
				(char*) &taobao::optimize_lv3_assign_to_quick_count,
				SHOW_LONGLONG }, //
		{ "optimize_lv3_assign_to_slow_count",
				(char*) &taobao::optimize_lv3_assign_to_slow_count,
				SHOW_LONGLONG }, //
		{ "active_slow_read_thread_num",
				(char*) &taobao::active_slow_read_thread_num, SHOW_INT }, //
		{ "throttle_count", (char*) &taobao::throttle_count, SHOW_LONGLONG }, //
		{ "slow_read_done_count", (char*) &taobao::slow_read_io_num,
				SHOW_LONGLONG }, //
        { "io_status", (char*) &io_status, SHOW_CHAR_PTR }, //
		{ NullS, NullS, SHOW_LONG } };


static void show_io_status(char* status, const size_t n) {
    easy_io_thread_t        *ioth;
    easy_thread_pool_t      *tp;
    int len = 0;
    memset(status, 0, n);


    len += snprintf(status + len, n - len, "[");
    tp=easy_io_var.io_thread_pool;

    easy_thread_pool_for_each(ioth, tp, 0) {
        int conn_count=0;
        easy_connection_t       *c, *c1;
        easy_spin_lock(&ioth->thread_lock);
        easy_list_for_each_entry_safe(c, c1, &ioth->connected_list, conn_list_node) {
            conn_count++;
        }
        easy_spin_unlock(&ioth->thread_lock);
        len += snprintf(status + len, n - len, "%d,",conn_count);

    }
    len--; //除去最后一个逗号
    snprintf(status + len, n - len, "]");

}

static int show_tdhs_vars(THD *thd, SHOW_VAR *var, char *buff) {
	taobao::show_optimize_status(_optimize_status, sizeof(_optimize_status));
    show_io_status(_io_status,sizeof(_io_status));
	var->type = SHOW_ARRAY;
	var->value = (char *) &hs_status_variables;
	return 0;
}

static SHOW_VAR daemon_tdh_socket_status_variables[] = { { "Tdhs",
		(char*) show_tdhs_vars, SHOW_FUNC }, { NullS, NullS, SHOW_LONG } };

struct st_mysql_daemon tdh_socket_plugin = { MYSQL_DAEMON_INTERFACE_VERSION };
/*
 Plugin library descriptor
 */mysql_declare_plugin (tdh_socket) { MYSQL_DAEMON_PLUGIN, &tdh_socket_plugin,
"tdh_socket", "wentong@taobao.com", "proxy the handler",
PLUGIN_LICENSE_GPL, tdh_socket_plugin_init, /* Plugin Init */
tdh_socket_plugin_deinit, /* Plugin Deinit */
0x0003 /* 0.3 */, daemon_tdh_socket_status_variables, /* status variables                */
daemon_tdh_socket_system_variables, /* system variables                */
NULL /* config options                  */
}
mysql_declare_plugin_end;
