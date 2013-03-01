/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_dbcontext.cpp
 *
 *  Created on: 2011-9-30
 *      Author: wentong
 */
#include "easy_io.h"
#include "easy_pool.h"

#include "util.hpp"
#include "debug_util.hpp"

#include "tdh_socket_dbcontext.hpp"
#include "tdh_socket_connection_context.hpp"
#include "tdh_socket_decode_request_binary.hpp"
#include "tdh_socket_decode_request_binary_v2.hpp"
#include "tdh_socket_share.hpp"
#include "tdh_socket_encode_response.hpp"
#include "tdh_socket_error.hpp"
#include "tdh_socket_thd.hpp"
#include "tdh_socket_dbutil.hpp"
#include "tdh_socket_optimize.hpp"
#include "tdh_socket_monitor.hpp"
#include "tdh_socket_config.hpp"
#include "tdh_socket_statistic.hpp"
#include "tdh_socket_bloom_filter.h"

#include "mysql_inc.hpp"

#define REQUEST_HASH_LEN 10

#define DBCONTEXT_MAX_CACHE_LOCK_TABLES 128

#define MAX_POOL_SIZE_FOR_OPEN_TABLE (sizeof(opened_table_t)*128)

namespace taobao {

const char* PRIMARY = PRIMARY_STRING;

decode_request_t* decode_request_array[TDHS_PROTOCOL_END] = { NULL,
		decode_request_by_binary ,decode_request_by_binary_v2};

typedef int (create_response_handler_t)(tdhs_table_t& tdhs_table,
		easy_request_t *req);

typedef int (process_data_handler_t)(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error);

typedef void (end_response_handler_t)(tdhs_table_t& tdhs_table,
		easy_request_t *req);

typedef struct cached_table_t cached_table_t;

struct cached_table_t {
	TABLE *table;
	uint64_t hash_code_for_table;
};

class tdhs_dbcontext: public tdhs_dbcontext_i, private noncopyable {
	friend class table_locker_for_context;
public:
	tdhs_dbcontext();
	virtual ~tdhs_dbcontext();
	virtual int init(tdhs_optimize_t _type, const void *stack_bottom);
	virtual tdhs_optimize_t get_type();
	virtual unsigned long get_use_steam_count();
	virtual bool need_write();
	virtual int destory();
	virtual void open_table(tdhs_request_t &req);
	virtual void lock_table();
	virtual int unlock_table();
	virtual void close_table();
	virtual void close_cached_table();
	virtual int execute(easy_request_t *r);
	virtual void set_thd_info(unsigned int bulk_request_num);
	virtual void using_stream();
	virtual void set_group_commit(bool gc);
	virtual time_t* get_thd_time();

    virtual bool need_close_table();
    virtual void set_need_close_table(bool need);
#ifdef TDHS_ROW_CACHE
	virtual bool is_in_cache(tdhs_request_t &req);
#endif
private:
	void lock_table(TABLE** tables, uint count);
	int unlock_table(opened_table_t** tables, size_t count);
	int do_find(easy_request_t *req, tdhs_client_wait_t &client_wait,
			create_response_handler_t *create_handler,
			process_data_handler_t *process_handler,
			end_response_handler_t *end_handler);
	int do_insert(easy_request_t *req);
	int do_batch(easy_request_t *req);
	int do_batch_with_lock(easy_request_t *req);

    void set_process_info(uint32_t id ,easy_request_t *req);

private:
	tdhs_optimize_t type;
	easy_pool_t* pool;
	easy_pool_t* pool_for_open_table;
	THD *thd;
	MYSQL_LOCK *lock;
	char info[MAX_INFO_SIZE];
	unsigned long long execute_count;
	int write_error;
	unsigned long use_stream_count;
	TABLE *last_table;
	bool bulk;
	bool in_batch;
	int retcode;
	const unsigned int cache_table_num;
	easy_hash_t* hash_table_for_opened;
	cached_table_t* cached_table;
	opened_table_t* for_cached_opened_table;
	unsigned int already_cached_table_num;
	unsigned int opened_table_num;
	unsigned int last_opened_table_num;
	TABLE* need_lock_tables[DBCONTEXT_MAX_CACHE_LOCK_TABLES];
	TABLE* current_table;

    char process_info[MAX_INFO_SIZE];
    bool need_close_cached_table;
};

tdhs_dbcontext::tdhs_dbcontext() :
        write_error(0), use_stream_count(0), bulk(true), in_batch(false),
        retcode(EASY_OK), cache_table_num(tdhs_cache_table_num_for_thd),
        already_cached_table_num(0), opened_table_num(0), last_opened_table_num(0),
        current_table(NULL),need_close_cached_table(false) {
}
tdhs_dbcontext::~tdhs_dbcontext() {
}

tdhs_dbcontext_i* tdhs_dbcontext_i::create() {
	return new tdhs_dbcontext();
}

#define TAOBAO_THR_OFFSETOF(fld) ((char *)(&thd->fld) - (char *)thd)

int tdhs_dbcontext::init(tdhs_optimize_t _type, const void *stack_bottom) {
	pool = easy_pool_create(0);
	pool_for_open_table = NULL;
	memset(need_lock_tables, 0,
			sizeof(TABLE*) * DBCONTEXT_MAX_CACHE_LOCK_TABLES);
	if (pool == NULL) {
		easy_error_log("TDHS:init dbconext mem pool failed");
		return EASY_ERROR;
	}
	type = _type;
	lock = NULL;
	execute_count = 0;
	hash_table_for_opened = easy_hash_create(pool, REQUEST_HASH_LEN,
			offsetof(opened_table_t,hash));
	if (hash_table_for_opened == NULL) {
		easy_error_log("TDHS:init dbconext hash_table_for_opened failed");
		easy_pool_destroy(pool);
		pool = NULL;
		return EASY_ERROR;
	}

	cached_table = (cached_table_t*) easy_pool_calloc(pool,
			sizeof(cached_table_t) * cache_table_num);

	if (cached_table == NULL) {
		easy_error_log("TDHS:init dbconext cached_table failed");
		easy_pool_destroy(pool);
		pool = NULL;
		return EASY_ERROR;
	}

	for_cached_opened_table = (opened_table_t *) easy_pool_calloc(pool,
			sizeof(opened_table_t) * cache_table_num);

	if (for_cached_opened_table == NULL) {
		easy_error_log("TDHS:init dbconext for_cached_opened_table failed");
		easy_pool_destroy(pool);
		pool = NULL;
		return EASY_ERROR;
	}

	easy_debug_log("TDHS:init thread!");
	char* db = NULL;
	if (type == TDHS_QUICK) {
		db = my_strdup("TDH_SOCKET_QUICK", MYF(0));
	} else if (type == TDHS_SLOW) {
		db = my_strdup("TDH_SOCKET_SLOW", MYF(0));
	} else if (type == TDHS_WRITE) {
		db = my_strdup("TDH_SOCKET_WRITE", MYF(0));
	} else {
		db = my_strdup("TDH_SOCKET", MYF(0));
	}
	thd = init_THD(db, stack_bottom, need_write());
	if (thd == NULL) {
		return EASY_ERROR;
	}
	easy_debug_log("TDHS:thd %p", thd);

	wait_server_to_start(thd);
	easy_debug_log("TDHS:init thd done!");
	thd_proc_info(thd, info);
	set_thread_message(info, "TDHS:listening");

	lex_start(thd);
	return EASY_OK;
}

tdhs_optimize_t tdhs_dbcontext::get_type() {
	return type;
}

unsigned long tdhs_dbcontext::get_use_steam_count() {
	return use_stream_count;
}
bool tdhs_dbcontext::need_write() {
	//MARK 写线程或事务线程 返回true
	return type == TDHS_WRITE;
}

int tdhs_dbcontext::destory() {
	easy_debug_log("TDHS:thd end %p", thd);
	destory_thd(thd);
	thd = NULL;
	if (pool) {
		easy_pool_destroy(pool);
		pool = NULL;
	}
	return EASY_OK;
}

static int compare_table_info(const void *a, const void *b) {
	const tdhs_request_table_t *table_info = (tdhs_request_table_t*) a;
	const opened_table_t *hashed_table = (opened_table_t *) b;
	const TABLE* table = (TABLE*) hashed_table->mysql_table;
	if (table_info->db.strlen() == table->s->db.length
			&& table_info->table.strlen() == table->s->table_name.length) {
		if (strncmp(table_info->db.str, table->s->db.str, table->s->db.length)
				== 0
				&& strncmp(table_info->table.str, table->s->table_name.str,
						table->s->table_name.length) == 0) {
			return 0;
		}

	}
	return 1;
}

void tdhs_dbcontext::set_thd_info(unsigned int bulk_request_num) {
	thd_proc_info(thd, info);
	set_thread_message(info,
			"TDHS:execute [%llu] opened table [%d] cached table [%d] bulk request [%d] ",
			execute_count, last_opened_table_num, already_cached_table_num,
			bulk_request_num);
}

void tdhs_dbcontext::using_stream() {
	thd_proc_info(thd, info);
	set_thread_message(info, "TDHS:send stream[%lu]", ++use_stream_count);
}

void tdhs_dbcontext::set_process_info(uint32_t id ,easy_request_t *req){
    char buffer[32];
    int len = snprintf(process_info, sizeof(process_info), "from:[%s] id:[%d]",
             easy_inet_addr_to_str(&req->ms->c->addr, buffer, 32) ,id);
    thd->set_query(process_info, len);
}

void tdhs_dbcontext::set_group_commit(bool gc) {
	if (need_write()) {
		bulk = gc;
	}
}

time_t* tdhs_dbcontext::get_thd_time() {
	return &thd->start_time;
}

bool tdhs_dbcontext::need_close_table(){
    return need_close_cached_table;
}

void tdhs_dbcontext::set_need_close_table(bool need){
    need_close_cached_table = need;
}

void tdhs_dbcontext::open_table(tdhs_request_t &req) {
	//MARK 针对request type 需要进行判断,不需要open table的直接退出
	if (req.type == REQUEST_TYPE_BATCH) {
		return;
	}
	if (pool_for_open_table == NULL) {
		pool_for_open_table = easy_pool_create(MAX_POOL_SIZE_FOR_OPEN_TABLE);
	}
	if (already_cached_table_num > 0) {
		tb_assert(already_cached_table_num<=cache_table_num);
		for (unsigned int i = 0; i < already_cached_table_num; i++) {
			cached_table_t &cached_t = cached_table[i];
			if (cached_t.table != NULL) {
				TABLE* t = cached_t.table;
				opened_table_t& opened_t = for_cached_opened_table[i];
				opened_t.mysql_table = t;
				easy_hash_add(hash_table_for_opened,
						cached_t.hash_code_for_table > 0 ?
								cached_t.hash_code_for_table :
								make_hash_code_for_table(t->s->db.str,t->s->db.length,t->s->table_name.str,t->s->table_name.length)
								, &opened_t.hash);
				if (opened_table_num < DBCONTEXT_MAX_CACHE_LOCK_TABLES) {
					need_lock_tables[opened_table_num++] = t;
				} else {
					opened_table_num++;
				}
				cached_t.table = NULL; //置空
				cached_t.hash_code_for_table = 0; //置空
			}
		}
		already_cached_table_num = 0; //置0
	}

	{
		opened_table_t * opened_table = (opened_table_t *) easy_hash_find_ex(
				hash_table_for_opened, req.table_info.hash_code_table(),
				compare_table_info, &req.table_info);
		if (opened_table != NULL) {
			req.opened_table = opened_table;
		} else {
			opened_table = (opened_table_t *) easy_pool_calloc(
					pool_for_open_table, sizeof(opened_table_t));
			if (opened_table != NULL) {
				req.opened_table = opened_table;
				TABLE* t = tdhs_open_table(thd, req.table_info,
						this->need_write());
				if (t != NULL) {
					req.opened_table->mysql_table = t;
					easy_hash_add(hash_table_for_opened,
							req.table_info.hash_code_table(),
							&req.opened_table->hash);
					if (opened_table_num < DBCONTEXT_MAX_CACHE_LOCK_TABLES) {
						need_lock_tables[opened_table_num++] = t;
					} else {
						opened_table_num++;
					}
                } else {
                    //下次loop的时候 会close table 防止 创建相同名字的表的时候会hang
                   set_need_close_table(true);
                }
			} else {
				//if opened_table==NULL because of memory is not enough
				easy_error_log(
						"TDHS:not enough memory for calloc opened_table_t!");
			}
		}

	}
}

//only use for bulk request
void tdhs_dbcontext::lock_table() {
	tb_assert(lock==NULL);
	TABLE** tables;
	bool is_malloc = false;
	if (!bulk) {
		return;
	}

	if (opened_table_num <= DBCONTEXT_MAX_CACHE_LOCK_TABLES) {
		tables = need_lock_tables;
	} else {
		tables = (TABLE**) TAOBAO_MALLOC(sizeof(TABLE*)*opened_table_num);
		if (tables == NULL) {
			easy_error_log("TDHS: lock_table failed for not enough memory!");
			return;
		}
		is_malloc = true;
		opened_table_t * t;
		uint32_t i = 0;
		uint32_t j = 0;
		easy_hash_list_t * node;
		easy_hash_for_each(i,node,hash_table_for_opened) {
			t =
					(opened_table_t*) ((char*) node
							- hash_table_for_opened->offset);
			tables[j++] = (TABLE*) t->mysql_table;
		}

		tb_assert(opened_table_num==j);
	}

	lock = tdhs_lock_table(thd, tables, opened_table_num, this->need_write());
	last_table = NULL;
	if (is_malloc) {
		TAOBAO_FREE(tables);
	}
}
//only use for none bulk request
void tdhs_dbcontext::lock_table(TABLE** tables, uint count) {
	if (bulk || in_batch) {
		return;
	}
	lock = tdhs_lock_table(thd, tables, count, this->need_write());
	last_table = NULL;
}

//only use for bulk request
int tdhs_dbcontext::unlock_table() {
	if (!bulk) {
		return EASY_OK;
	}
	int ret = tdhs_unlock_table(thd, &lock, hash_table_for_opened,
			this->need_write(), write_error);
	write_error = 0;
	last_table = NULL;
	return ret;
}

//only use for none bulk request
int tdhs_dbcontext::unlock_table(opened_table_t** tables, size_t count) {
	if (bulk || in_batch) {
		return EASY_OK;
	}
	int ret = tdhs_unlock_table_lite(thd, &lock, tables, count,
			this->need_write(), write_error);
	write_error = 0;
	last_table = NULL;
	return ret;
}

static TDHS_INLINE void clean_hash_table(easy_hash_t* table) {
	table->count = 0;
	table->seqno = 1;
	memset(table->buckets, 0, table->size * sizeof(easy_hash_list_t *));
	easy_list_init(&table->list);
}

void tdhs_dbcontext::close_table() {
	last_opened_table_num = opened_table_num;
	if (opened_table_num == 0) {
		return;
	}
	if (tdhs_cache_table_on && opened_table_num <= cache_table_num) {
		opened_table_t * t;
		unsigned int j = 0;
		uint32_t i = 0;
		easy_hash_list_t * node;
		easy_hash_for_each(i,node,hash_table_for_opened) {
			t =
					(opened_table_t*) ((char*) node
							- hash_table_for_opened->offset);
			cached_table_t &cached_t = cached_table[j++];
			cached_t.table = (TABLE *) t->mysql_table;
			cached_t.hash_code_for_table = t->hash.key;
		}
		already_cached_table_num = opened_table_num;
	} else {
		tdhs_close_table(thd);
	}
	opened_table_num = 0;
	clean_hash_table(hash_table_for_opened);
	easy_pool_destroy(pool_for_open_table);
	pool_for_open_table = NULL;
}

void tdhs_dbcontext::close_cached_table() {
    //	tb_assert(tdhs_cache_table_on==0);
    tb_assert(opened_table_num==0);tb_assert(lock==NULL);
    easy_debug_log("TDHS:close_cached_table [%d]",
                   already_cached_table_num);
    tdhs_close_table(thd);
    already_cached_table_num = 0;
}

class table_locker_for_context: private noncopyable {
public:

	table_locker_for_context(tdhs_dbcontext *_context, opened_table_t *_table,
			tdhs_packet_t *_response) :
			context(_context), table(_table), response(_response) {
		TABLE * t = (TABLE*) (table->mysql_table);
		context->lock_table(&t, 1);
	}

	~table_locker_for_context() {
		if (context->unlock_table(&table, 1) != EASY_OK) {
			tb_assert(response!=NULL);
			if (response->command_id_or_response_code >= 200
					&& response->command_id_or_response_code < 300
					&& response->command_id_or_response_code
							!= CLIENT_STATUS_MULTI_STATUS) {
				context->retcode = tdhs_response_error(response,
						CLIENT_STATUS_SERVER_ERROR,
						CLIENT_ERROR_CODE_FAILED_TO_COMMIT);
			}
		}
	}
private:
	tdhs_dbcontext *context;
	opened_table_t *table;
	tdhs_packet_t *response;
};

int tdhs_dbcontext::execute(easy_request_t *r) {
	tdhs_client_wait_t client_wait = { false, false, false, this, { 0 } }; //for stream
	tdhs_packet_t *packet = (tdhs_packet_t*) ((r->ipacket));
    uint32_t seq_id = packet->seq_id;
	tdhs_request_t &request = packet->req;
	retcode = EASY_OK;
	if (thd->killed != THD::NOT_KILLED) {
		easy_info_log("[%d] need switch to NO_KILLED at start!", thd->killed);
		thd->killed = THD::NOT_KILLED;
	}
	thd->clear_error();
    set_process_info(seq_id, r);
	int ret;
	switch (request.type) {
	case REQUEST_TYPE_GET:
		easy_debug_log("TDHS:do_get");
		thd->lex->sql_command = SQLCOM_SELECT;
		ret = do_find(r, client_wait, create_response_for_data, response_record,
				NULL);
		statistic_increment(get_count, &LOCK_status);
		break;
	case REQUEST_TYPE_COUNT:
		easy_debug_log("TDHS:do_count");
		thd->lex->sql_command = SQLCOM_SELECT;
		ret = do_find(r, client_wait, create_response_for_count, count_record,
				end_count_record);
		statistic_increment(count_count, &LOCK_status);
		break;
	case REQUEST_TYPE_UPDATE:
		easy_debug_log("TDHS:do_update");
		thd->lex->sql_command = SQLCOM_UPDATE;
		tb_assert(need_write());
		ret = do_find(r, client_wait, create_response_for_update, update_record,
				end_update_record);
		statistic_increment(update_count, &LOCK_status);
		break;
	case REQUEST_TYPE_DELETE:
		easy_debug_log("TDHS:do_delete");
		tb_assert(need_write());
		thd->lex->sql_command = SQLCOM_DELETE;
		ret = do_find(r, client_wait, create_response_for_update, delete_record,
				end_update_record);
		statistic_increment(delete_count, &LOCK_status);
		break;
	case REQUEST_TYPE_INSERT:
		easy_debug_log("TDHS:do_insert");
		tb_assert(need_write());
		thd->lex->sql_command = SQLCOM_INSERT;
		ret = do_insert(r);
		statistic_increment(insert_count, &LOCK_status);
		break;
	case REQUEST_TYPE_BATCH:
		easy_debug_log("TDHS:do_batch");
		tb_assert(need_write());
		ret = bulk ? do_batch(r) : do_batch_with_lock(r);
		statistic_increment(batch_count, &LOCK_status);
		break;
	default:
		tdhs_packet_t *response = (tdhs_packet_t*) ((r->opacket));
		ret = tdhs_response_error_global(response,
				CLIENT_STATUS_NOT_IMPLEMENTED,
				CLIENT_ERROR_CODE_NOT_IMPLEMENTED);
		break;
	}
	execute_count++;
	use_stream_count = 0;
	add_optimize(request);
	if (client_wait.is_inited) {
		if (!client_wait.is_closed) {
			//此时request已经被destory
			r->args = NULL;
		}
		easy_client_wait_cleanup(&client_wait.client_wait);
	}
	if (thd->killed != THD::NOT_KILLED) {
		easy_info_log("[%d] need switch to NO_KILLED at end!", thd->killed);
		thd->killed = THD::NOT_KILLED;
	}
    thd->reset_query();
	return retcode != EASY_OK ? retcode : ret;
}

TDHS_INLINE int tdhs_dbcontext::do_find(easy_request_t *req,
		tdhs_client_wait_t &client_wait,
		create_response_handler_t *create_handler,
		process_data_handler_t *process_handler,
		end_response_handler_t *end_handler) {
	tdhs_packet_t *packet = (tdhs_packet_t*) ((req->ipacket));
	tdhs_request_t &request = packet->req;
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	tdhs_table_t tdhs_table;
	memset(&tdhs_table, 0, sizeof(tdhs_table_t));
	tdhs_table.thd = this->thd;
	int ret;
	if (request.opened_table == NULL) {
		return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
				CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY);
	}
	if (request.opened_table->mysql_table == NULL) {
		return tdhs_response_error(response, CLIENT_STATUS_NOT_FOUND,
				CLIENT_ERROR_CODE_FAILED_TO_OPEN_TABLE);
	}
	tdhs_table.table = (TABLE *) request.opened_table->mysql_table;

	table_locker_for_context _locker(this, request.opened_table, response);

	//get lock
	if (this->lock == NULL) {
		return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
				CLIENT_ERROR_CODE_FAILED_TO_LOCK_TABLE);
	}

	if ((ret = tdhs_open_index(tdhs_table, request.table_info)) != EASY_OK) {
		return tdhs_response_error(response, CLIENT_STATUS_NOT_FOUND,
				CLIENT_ERROR_CODE_FAILED_TO_OPEN_INDEX);
	}

	if (parse_field(tdhs_table, request.table_info) != EASY_OK) {
		return tdhs_response_error(response, CLIENT_STATUS_NOT_FOUND,
				CLIENT_ERROR_CODE_FAILED_TO_MISSING_FIELD);
	}

	//get find_flag
	ha_rkey_function find_flag = HA_READ_KEY_EXACT;
	switch (request.get.find_flag) {
	case TDHS_EQ:
	case TDHS_DEQ:
		find_flag = HA_READ_KEY_EXACT;
		break;
	case TDHS_GE:
    case TDHS_BETWEEN:
		find_flag = HA_READ_KEY_OR_NEXT;
		break;
	case TDHS_LE:
		find_flag = HA_READ_KEY_OR_PREV;
		break;
	case TDHS_GT:
		find_flag = HA_READ_AFTER_KEY;
		break;
	case TDHS_LT:
		find_flag = HA_READ_BEFORE_KEY;
		break;
	case TDHS_IN:
		find_flag = HA_READ_KEY_EXACT;
		break;
	default:
		//can't be happeded ,because it vailded in decode
		tb_assert(FALSE);
		break;
	}

	//filter init
	uchar *filter_buf = NULL;
	size_t filter_buf_size = 0;
	if (request.get.filter.filter_num > 0) {
		if (parse_filter(tdhs_table, request.get.filter) != EASY_OK) {
			return tdhs_response_error(response, CLIENT_STATUS_NOT_FOUND,
					CLIENT_ERROR_CODE_FAILED_TO_MISSING_FIELD_IN_FILTER_OR_USE_BLOB);
		}
		filter_buf_size = calc_filter_buf_size(tdhs_table, request.get.filter);
	}
	stack_liker _filter_buf_stack(filter_buf_size);
	filter_buf = (uchar *) _filter_buf_stack.get_ptr();
	if (filter_buf == NULL) {
		return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
				CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY);
	}
	if (filter_buf_size > 0) {
		fill_filter_buf(tdhs_table, request.get.filter, filter_buf,
				filter_buf_size);
	}
	//filter init end
	{
        key_range between_start_key;
        key_range between_end_key;

		KEY& kinfo = tdhs_table.table->key_info[tdhs_table.idxnum];
		stack_liker _key_buf_stack(kinfo.key_length);
        stack_liker _key_buf_stack_for_between_end(kinfo.key_length);

		uchar * const key_buf = (uchar *) _key_buf_stack.get_ptr();
        uchar * const key_buf_for_between_end = (uchar *) _key_buf_stack_for_between_end.get_ptr();
		if (key_buf == NULL) {
			return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
					CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY);
		}

		tdhs_init_response(response);

		if (create_handler && (*create_handler)(tdhs_table, req) != EASY_OK) {
			return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
					CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY);
		}

		uint32_t in_index = 0;
		tb_assert(request.get.key_num>0);
		//默认取第一个key 适合任何场景
		if ((ret = parse_index(tdhs_table, request.get.keys[in_index].key,
				request.get.keys[in_index].key_field_num))
				!= TDHS_PARSE_INDEX_DONE) {
			return tdhs_response_error(response, CLIENT_STATUS_NOT_FOUND,
					static_cast<tdhs_client_error_code_t>(ret));
		}

		size_t key_len_sum = prepare_keybuf(request.get.keys[in_index].key,
				request.get.keys[in_index].key_field_num, key_buf,
				tdhs_table.table, kinfo);

        if(request.get.find_flag==TDHS_BETWEEN){
           between_start_key.key = key_buf;
           between_start_key.length = key_len_sum;
           between_start_key.keypart_map = (1U
                                            << request.get.keys[in_index].key_field_num) - 1;
           between_start_key.flag = find_flag;

           if (key_buf_for_between_end == NULL) {
               return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
                       CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY);
           }

           //默认取第二个key 作为范围查询的结束key
           if ((ret = parse_index(tdhs_table, request.get.keys[1].key,
                   request.get.keys[1].key_field_num))
                   != TDHS_PARSE_INDEX_DONE) {
               return tdhs_response_error(response, CLIENT_STATUS_NOT_FOUND,
                       static_cast<tdhs_client_error_code_t>(ret));
           }

           size_t key_len_sum_for_between_end = prepare_keybuf(request.get.keys[1].key,
                   request.get.keys[1].key_field_num, key_buf_for_between_end,
                   tdhs_table.table, kinfo);

           between_end_key.key = key_buf_for_between_end;
           between_end_key.length = key_len_sum_for_between_end;
           between_end_key.keypart_map = (1U
                                            << request.get.keys[1].key_field_num) - 1;
           between_end_key.flag = HA_READ_KEY_EXACT; //确保最后为<=

        }

		/* handler */
		tdhs_table.table->read_set = &tdhs_table.table->s->all_set;
		handler * const hnd = tdhs_table.table->file;
//		if (!context->need_write()) {
//			hnd->init_table_handle_for_HANDLER();
////			hnd->prebuilt->used_in_HANDLER = FALSE;
//		}
		hnd->ha_index_or_rnd_end();
		hnd->ha_index_init(tdhs_table.idxnum, 1);
		bool is_first = true;
		uint32_t limit = request.get.limit;
		bool is_unlimited = request.get.limit ? false : true;
		uint32_t skip = request.get.start;
		int r = 0;
		while (is_unlimited || limit != 0) {
			if (this->thd->killed != THD::NOT_KILLED) {
				hnd->ha_index_or_rnd_end();
				return tdhs_response_error(response,
						CLIENT_STATUS_SERVICE_UNAVAILABLE,
						CLIENT_ERROR_CODE_KILLED);
			}
			if (is_first) {
				is_first = false;
				const key_part_map kpm = (1U
						<< request.get.keys[in_index].key_field_num) - 1;
				if (request.get.find_flag == TDHS_DEQ) {
					r = hnd->index_read_last_map(tdhs_table.table->record[0],
							key_buf, kpm);
                }else if(request.get.find_flag==TDHS_BETWEEN){
                    r = hnd->read_range_first(&between_start_key,
                                              &between_end_key,
                                              0, 1);
                }else {
					r = hnd->index_read_map(tdhs_table.table->record[0],
							key_buf, kpm, find_flag);
				}
			} else {
				switch (find_flag) {
				case HA_READ_BEFORE_KEY:
				case HA_READ_KEY_OR_PREV:
					r = hnd->index_prev(tdhs_table.table->record[0]);
					break;
				case HA_READ_AFTER_KEY:
				case HA_READ_KEY_OR_NEXT:
                    if(request.get.find_flag==TDHS_BETWEEN){
                        r = hnd->read_range_next();
                    }else{
                        r = hnd->index_next(tdhs_table.table->record[0]);
                    }
					break;
				case HA_READ_KEY_EXACT:
					if (request.get.find_flag == TDHS_DEQ) {
						r = index_prev_same(hnd, tdhs_table.table, key_buf,
								key_len_sum);
					} else {
						r = hnd->index_next_same(tdhs_table.table->record[0],
								key_buf, key_len_sum);
					}
					break;
				default:
					r = HA_ERR_END_OF_FILE; /* to finish the loop */
					break;
				}
			}

			if (r != 0 && r != HA_ERR_RECORD_DELETED) {
				if (request.get.find_flag == TDHS_IN
						&& ++in_index < request.get.key_num
						&& (r == HA_ERR_END_OF_FILE || r == HA_ERR_KEY_NOT_FOUND)) {
					//处理 in的时候的逻辑
					is_first = true;
					if ((ret = parse_index(tdhs_table,
							request.get.keys[in_index].key,
							request.get.keys[in_index].key_field_num))
							!= TDHS_PARSE_INDEX_DONE) {
						hnd->ha_index_or_rnd_end();
						return tdhs_response_error(response,
								CLIENT_STATUS_NOT_FOUND,
								static_cast<tdhs_client_error_code_t>(ret));
					}

					key_len_sum = prepare_keybuf(request.get.keys[in_index].key,
							request.get.keys[in_index].key_field_num, key_buf,
							tdhs_table.table, kinfo);
					continue;
				}
				//get some error
				break;
			} else if (r == HA_ERR_RECORD_DELETED) {
				continue;
			}

			//filter record
			if (request.get.filter.filter_num > 0) {
				if (!filter_record(tdhs_table, request.get.filter,
						filter_buf)) {
					continue;
				}
			}

			if (!is_unlimited && skip == 0) {
				limit--;
			}
			if (skip == 0) {
				if (tdhs_log_level >= EASY_LOG_DEBUG) {
					dump_record(tdhs_table);
				}
				int process_ret;
				int process_db_ret = 0;
				if (process_handler
						&& (process_ret = (*process_handler)(tdhs_table, req,
								client_wait, &process_db_ret,
								&(this->write_error))) != EASY_OK) {
					hnd->ha_index_or_rnd_end();
					return process_ret;
				}
				if (process_db_ret != 0
						&& process_db_ret != HA_ERR_RECORD_DELETED) {
					//get some error in process_handler
					r = process_db_ret;
					break;
				}
			}
			if (skip > 0) {
				skip--;
			}

		}
		hnd->ha_index_or_rnd_end();

		if (r != 0 && r != HA_ERR_RECORD_DELETED && r != HA_ERR_KEY_NOT_FOUND
				&& r != HA_ERR_END_OF_FILE) {
			/* failed */
			easy_error_log("TDHS: read index error,code [%d]", r);
			return tdhs_response_error(response, CLIENT_STATUS_DB_ERROR, r);
		}
	}
	if (end_handler) {
		(*end_handler)(tdhs_table, req);
	}
	return EASY_OK;
}

TDHS_INLINE int tdhs_dbcontext::do_insert(easy_request_t *req) {
	tdhs_packet_t *packet = (tdhs_packet_t*) ((req->ipacket));
	tdhs_request_t &request = packet->req;
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	tdhs_table_t tdhs_table;
	memset(&tdhs_table, 0, sizeof(tdhs_table_t));
	tdhs_table.thd = this->thd;
	if (request.opened_table == NULL) {
		return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
				CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY);
	}
	if (request.opened_table->mysql_table == NULL) {
		return tdhs_response_error(response, CLIENT_STATUS_NOT_FOUND,
				CLIENT_ERROR_CODE_FAILED_TO_OPEN_TABLE);
	}
	tdhs_table.table = (TABLE *) request.opened_table->mysql_table;
	table_locker_for_context _locker(this, request.opened_table, response);
	//get lock
	if (this->lock == NULL) {
		return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
				CLIENT_ERROR_CODE_FAILED_TO_LOCK_TABLE);
	}

	if (parse_field(tdhs_table, request.table_info) != EASY_OK) {
		return tdhs_response_error(response, CLIENT_STATUS_NOT_FOUND,
				CLIENT_ERROR_CODE_FAILED_TO_MISSING_FIELD);
	}

	tdhs_init_response(response);

	if (write_update_header_to_response(*response, MYSQL_TYPE_LONG, 48,
			1) != EASY_OK) {
		return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
				CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY);
	}

	TABLE * const table = tdhs_table.table;
	handler * const hnd = table->file;
	uchar * const buf = table->record[0];
	empty_record(table);
	memset(buf, 0, table->s->null_bytes); /* clear null flags */
	for (size_t i = 0; i < tdhs_table.field_num; i++) {
		Field * const fld = table->field[tdhs_table.field_idx[i]];
		tdhs_string_t &value = request.values.value[i];
		switch (request.values.flag[i]) {
		case TDHS_UPDATE_SET:
		case TDHS_UPDATE_ADD:
		case TDHS_UPDATE_SUB:
			if (value.len == 0) {
				fld->set_null();
			} else {
				fld->set_notnull();
				if (fld->store(value.str, value.strlen(), &my_charset_bin)
						< 0) {
					easy_error_log(
							"TDHS:insert store error,field:[%s],value:[%s]",
							fld->field_name, value.str_print());
//					this->write_error = 1;
					return tdhs_response_error(response, CLIENT_STATUS_DB_ERROR,
							FIELD_STORE_ERROR);
				}
			}
			break;
		case TDHS_UPDATE_NOW:
			if (save_now(tdhs_table.thd, fld) < 0) {
				easy_error_log(
						"TDHS:insert store error,field:[%s],value:[now()]",
						fld->field_name);
//				this->write_error = 1;
				return tdhs_response_error(response, CLIENT_STATUS_DB_ERROR,
						FIELD_STORE_ERROR);
			}
			break;
		default:
			tb_assert(false);
			break;
		}
	}
	table->next_number_field = table->found_next_number_field;
	//如果表变了需要重置自增计数
	if (this->last_table != table) {
		if (this->last_table != NULL) {
			hnd->start_stmt(this->thd, TL_WRITE);
		}
		this->last_table = table;
	}
	const int r = hnd->ha_write_row(buf);
	const ulonglong insert_id = table->file->insert_id_for_cur_row;
	table->next_number_field = 0;
	if (r == 0) {
		request.opened_table->modified = true;
		write_insert_ender_to_response(*response, insert_id);
	} else {
//		this->write_error = 1;
		return tdhs_response_error(response, CLIENT_STATUS_DB_ERROR, r);
	}
	return EASY_OK;
}

#ifdef TDHS_ROW_CACHE
bool tdhs_dbcontext::is_in_cache(tdhs_request_t &req) {
	bool ret = false;
	tdhs_table_t tdhs_table;
	//类型不对
	if (req.status != TDHS_DECODE_DONE
			|| (req.type != REQUEST_TYPE_GET && req.type != REQUEST_TYPE_COUNT)
			|| req.get.find_flag != TDHS_EQ) {
		return ret;
	}
	//没有table
	if (req.opened_table == NULL || req.opened_table->mysql_table == NULL) {
		return ret;
	}
	memset(&tdhs_table, 0, sizeof(tdhs_table_t));
	tdhs_table.thd = thd;
	tdhs_table.table = (TABLE *) req.opened_table->mysql_table;
	//沒有index
	if (tdhs_open_index(tdhs_table, req.table_info) != EASY_OK) {
		return ret;
	}
	KEY& kinfo = tdhs_table.table->key_info[tdhs_table.idxnum];
	stack_liker _key_buf_stack(kinfo.key_length);
	uchar * const key_buf = (uchar *) _key_buf_stack.get_ptr();
	//没内存生成key_buf
	if (key_buf == NULL) {
		return ret;
	}
	//key 数目不匹配
	if (parse_index(tdhs_table, req.get.keys[0].key,
			req.get.keys[0].key_field_num) != TDHS_PARSE_INDEX_DONE) {
		return ret;
	}

	prepare_keybuf(req.get.keys[0].key, req.get.keys[0].key_field_num, key_buf,
			tdhs_table.table, kinfo);

	/* handler */
	tdhs_table.table->read_set = &tdhs_table.table->s->all_set;
	handler * const hnd = tdhs_table.table->file;
	hnd->init_table_handle_for_HANDLER();
	hnd->ha_index_or_rnd_end();
	hnd->ha_index_init(tdhs_table.idxnum, 1);
	const key_part_map kpm = (1U << req.get.keys[0].key_field_num) - 1;
	ret = hnd->ha_is_in_cache(key_buf, kpm);
	hnd->ha_index_or_rnd_end();
	return ret;
}
#endif

TDHS_INLINE int tdhs_dbcontext::do_batch(easy_request_t *req) {
	tdhs_packet_t *packet = (tdhs_packet_t*) ((req->ipacket));
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	tdhs_packet_t *batch_packet = packet->next;
	int ret = EASY_OK;
	easy_request_t fake_req;
	if (tdhs_response_batch(response) != EASY_OK) {
		return EASY_ERROR;
	}
	this->in_batch = true;
	while (batch_packet) {
		memset(&fake_req, 0, sizeof(easy_request_t)); //重置
		fake_req.ms = req->ms;
		fake_req.ipacket = batch_packet;
		fake_req.opacket = batch_packet;
		fake_req.args = req->args;
		if ((ret = this->execute(&fake_req)) != EASY_OK) {
			this->write_error = 1; //失败需要全部回滚
			break;
		}
		if (batch_packet->command_id_or_response_code >= 300) {
			this->write_error = 1; //有失败的请求 需要全部回滚
		}
		batch_packet = batch_packet->next;
	}
	this->in_batch = false;
	return ret;
}

TDHS_INLINE int tdhs_dbcontext::do_batch_with_lock(easy_request_t *req) {
	tb_assert(!this->bulk);
	tdhs_packet_t *packet = (tdhs_packet_t*) ((req->ipacket));
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	tdhs_packet_t *batch_packet = packet->next;
	uint32_t batch_num = packet->reserved;
	int ret;
	size_t size = sizeof(void*) * batch_num * 2;
	stack_liker _table_heap(size);
	void** mysql_tables = (void**) _table_heap.get_ptr();
	if (mysql_tables == NULL) {
		return tdhs_response_error(response, CLIENT_STATUS_SERVER_ERROR,
				CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY);
	}
	opened_table_t** opened_tables = (opened_table_t**) (mysql_tables
			+ batch_num);
	size_t bloom_fsize = FSIZE(256);
	char easy_table_bloom[bloom_fsize];
	memset(easy_table_bloom, 0, sizeof(easy_table_bloom));
	size_t i = 0;
	while (batch_packet) {
		opened_table_t * ot = batch_packet->req.opened_table;
		if (ot != NULL && ot->mysql_table != NULL) {
			bool is_container = false;
			if (GETBIT(easy_table_bloom, (uint64_t)ot % bloom_fsize)) {
				for (size_t j = 0; j < i; j++) {
					if (opened_tables[j] == ot) {
						is_container = true;
						break;
					}
				}
			}
			if (!is_container) {
				opened_tables[i] = ot;
				mysql_tables[i++] = ot->mysql_table;
				SETBIT(easy_table_bloom, (uint64_t)ot % bloom_fsize);
			}
		}
		batch_packet = batch_packet->next;
	}tb_assert(i<=batch_num);
	this->lock_table((TABLE**) mysql_tables, i);
	ret = do_batch(req);
	if (this->unlock_table(opened_tables, i) != EASY_OK && ret == EASY_OK) {
		tdhs_packet_t *rp = response;
		while (rp) {
			if (rp->command_id_or_response_code >= 200
					&& rp->command_id_or_response_code < 300
					&& rp->command_id_or_response_code
							!= CLIENT_STATUS_MULTI_STATUS) {
				if (tdhs_response_error(rp, CLIENT_STATUS_SERVER_ERROR,
						CLIENT_ERROR_CODE_FAILED_TO_COMMIT) != EASY_OK) {
					return EASY_ERROR;
				}
			}
			rp = rp->next;
		}
	}
	return ret;
}

} // namespace taobao

