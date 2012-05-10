/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_dbutil.hpp
 *
 *  Created on: 2011-10-25
 *      Author: wentong
 */

#ifndef TDH_SOCKET_DBUTIL_HPP_
#define TDH_SOCKET_DBUTIL_HPP_

#include "tdh_socket_define.hpp"
#include "tdh_socket_error.hpp"
#include "tdh_socket_config.hpp"
#include "tdh_socket_statistic.hpp"
#include "tdh_socket_encode_response.hpp"
#include "mysql_inc.hpp"

namespace taobao {

typedef struct tdhs_table_t tdhs_table_t;

struct tdhs_table_t {
	TABLE *table;
	size_t idxnum;
	uint32_t field_num;
	size_t field_idx[REQUEST_MAX_FIELD_NUM];
	char field_type[REQUEST_MAX_FIELD_NUM];
	THD *thd;
	uint32_t update_row_num;
	uint32_t change_row_num;
};

static TDHS_INLINE int parse_field(tdhs_table_t &tdhs_table,
		const tdhs_request_table_t &request_table);

static TDHS_INLINE int parse_filter(tdhs_table_t &tdhs_table,
		tdhs_filter_t &filter);

static TDHS_INLINE size_t calc_filter_buf_size(tdhs_table_t &tdhs_table,
		tdhs_filter_t &filter);

static TDHS_INLINE void fill_filter_buf(tdhs_table_t &tdhs_table,
		tdhs_filter_t &filter, uchar *filter_buf, size_t len);

#define TDHS_PARSE_INDEX_DONE EASY_OK
#define TDHS_PARSE_INDEX_FAILED_CANNOT_MATCH_KEY_NUM CLIENT_ERROR_CODE_FAILED_TO_MATCH_KEY_NUM

static TDHS_INLINE int parse_index(tdhs_table_t &tdhs_table,
		const tdhs_string_t* key, const uint32_t key_num);

static TDHS_INLINE TABLE * tdhs_open_table(THD * thd,
		const tdhs_request_table_t &request_table, const bool need_write);

static TDHS_INLINE int tdhs_open_index(tdhs_table_t& tdhs_table,
		const tdhs_request_table_t &request_table);

static TDHS_INLINE void tdhs_close_table(THD *thd);

static TDHS_INLINE MYSQL_LOCK * tdhs_lock_table(THD *thd, TABLE** tables,
		uint count, bool for_write_flag);

static TDHS_INLINE int tdhs_unlock_table(THD *thd, MYSQL_LOCK** lock,
		easy_hash_t* opened_table, bool for_write_flag, int write_error);

static TDHS_INLINE int tdhs_unlock_table_lite(THD *thd, MYSQL_LOCK** lock,
		opened_table_t** opened_table, size_t opend_table_size,
		bool for_write_flag, int write_error);

static TDHS_INLINE size_t prepare_keybuf(const tdhs_string_t* keys,
		const uint32_t key_num, uchar *key_buf, TABLE *table, KEY& kinfo);

static TDHS_INLINE void dump_record(tdhs_table_t& tdhs_table);

static TDHS_INLINE void dump_field(Field* const field);

static TDHS_INLINE int filter_record(tdhs_table_t &tdhs_table,
		tdhs_filter_t &filter, uchar *filter_buf);

static TDHS_INLINE int create_response_for_data(tdhs_table_t& tdhs_table,
		easy_request_t *req);

static TDHS_INLINE int response_record(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error);

static TDHS_INLINE int save_now(THD *thd, Field *field);

static TDHS_INLINE int create_response_for_update(tdhs_table_t& tdhs_table,
		easy_request_t *req);

static TDHS_INLINE int create_response_for_count(tdhs_table_t& tdhs_table,
		easy_request_t *req);

static TDHS_INLINE int update_record(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error);

static TDHS_INLINE int count_record(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error);

static TDHS_INLINE int delete_record(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error);

static TDHS_INLINE void end_update_record(tdhs_table_t& tdhs_table,
		easy_request_t *req);

static TDHS_INLINE void end_count_record(tdhs_table_t& tdhs_table,
		easy_request_t *req);

static TDHS_INLINE int index_prev_same(handler * const hnd, TABLE *table,
		const uchar *key, uint keylen);

static TDHS_INLINE int parse_field(tdhs_table_t &tdhs_table,
		const tdhs_request_table_t &request_table) {
	TABLE *table = tdhs_table.table;
	for (uint32_t i = 0; i < request_table.field_num; i++) {
		Field **fld = NULL;
		size_t j = 0;
		for (fld = table->field; *fld; ++fld, ++j) {
			if (request_table.fields[i].len > 0
					&& strcasecmp(request_table.fields[i].str,
							(*fld)->field_name) == 0) {
				tdhs_table.field_idx[tdhs_table.field_num] = j;
				tdhs_table.field_type[tdhs_table.field_num] =
						(((int) (*fld)->type()) & 0xFF);
				tdhs_table.field_num++;
				break;
			}
		}
		if (i == tdhs_table.field_num) {
			easy_warn_log( "TDHS: can't find field [%s] in table [%s] [%s]",
					request_table.fields[i].str_print(), request_table.db.str_print(), request_table.table.str_print());
			return EASY_ERROR;
		}
	}
	return EASY_OK;
}

static TDHS_INLINE int parse_filter(tdhs_table_t &tdhs_table,
		tdhs_filter_t &filter) {
	TABLE *table = tdhs_table.table;
	uint32_t field_num = 0;
	for (uint32_t i = 0; i < filter.filter_num; i++) {
		Field **fld = NULL;
		size_t j = 0;
		for (fld = table->field; *fld; ++fld, ++j) {
			if (filter.field[i].len > 0
					&& strcasecmp(filter.field[i].str, (*fld)->field_name)
							== 0) {
				if (((*fld)->flags & BLOB_FLAG) != 0) {
					easy_warn_log(
							"TDHS: can't support blob field [%s] in filter",
							filter.field[i].str_print());
					return EASY_ERROR; //不支持blob字段的filter
				}
				filter.field_idx[field_num++] = j;
				break;
			}
		}
		if (i == field_num) {
			easy_warn_log( "TDHS: can't find field [%s] in filter",
					filter.field[i].str_print());
			return EASY_ERROR;
		}
	}
	return EASY_OK;
}

static TDHS_INLINE size_t calc_filter_buf_size(tdhs_table_t &tdhs_table,
		tdhs_filter_t &filter) {
	size_t filter_buf_len = 0;
	TABLE *table = tdhs_table.table;
	for (uint32_t i = 0; i < filter.filter_num; i++) {
		if (filter.value[i].len == 0) {
			continue;
		}
		Field* const field = table->field[filter.field_idx[i]];
		filter_buf_len += field->pack_length();
	}
	filter_buf_len++;
	/* Field_medium::cmp() calls uint3korr(), which may read 4 bytes.
	 Allocate 1 more byte for safety. */
	return filter_buf_len;
}

static TDHS_INLINE void fill_filter_buf(tdhs_table_t &tdhs_table,
		tdhs_filter_t &filter, uchar *filter_buf, size_t len) {
	size_t pos = 0;
	TABLE *table = tdhs_table.table;
	for (uint32_t i = 0; i < filter.filter_num; i++) {
		if (filter.value[i].len == 0) {
			continue;
		}
		Field* const field = table->field[filter.field_idx[i]];
		if (filter.value[i].len == 0) {
			field->set_null();
		} else {
			field->store(filter.value[i].str, filter.value[i].strlen(),
					&my_charset_bin);
		}
		const size_t packlen = field->pack_length();
		memcpy(filter_buf + pos, field->ptr, packlen);
		pos += packlen;
	}
	tb_assert(len == pos||(len-1) == pos);
}

static TDHS_INLINE int parse_index(tdhs_table_t &tdhs_table,
		const tdhs_string_t* key, const uint32_t key_num) {
	TABLE *table = tdhs_table.table;
	KEY& kinfo = table->key_info[tdhs_table.idxnum];
	if (key_num > kinfo.key_parts) {
		easy_warn_log( "TDHS: parse index key_parts:real [%d] request [%d]\n",
				kinfo.key_parts, key_num);
		return TDHS_PARSE_INDEX_FAILED_CANNOT_MATCH_KEY_NUM;
	}
	return TDHS_PARSE_INDEX_DONE;
}

static TDHS_INLINE TABLE * tdhs_open_table(THD * thd,
		const tdhs_request_table_t &request_table, const bool need_write) {
	bool refresh = true;
	TABLE_LIST tables;
	TABLE *table = NULL;
#if MYSQL_VERSION_ID >= 50505
	tables.init_one_table(request_table.db.str, request_table.db.strlen(),
			request_table.table.str, request_table.table.strlen(),
			request_table.table.str, need_write ? TL_WRITE : TL_READ);
	tables.mdl_request.init(MDL_key::TABLE, request_table.db.str,
			request_table.table.str,
			need_write ? MDL_SHARED_WRITE : MDL_SHARED_READ, MDL_TRANSACTION);
	Open_table_context ot_act(thd, 0);
	if (!open_table(thd, &tables, thd->mem_root, &ot_act)) {
		table = tables.table;
	}
#else
	tables.init_one_table(request_table.db.str, request_table.table.str,
			need_write ? TL_WRITE : TL_READ);
	table = open_table(thd, &tables, thd->mem_root, &refresh,
			OPEN_VIEW_NO_PARSE);
#endif

	if (table == NULL) {
		easy_warn_log( "TDHS: failed to open table %p [%s] [%s] [%d]\n",
				thd, request_table.db.str_print(), request_table.table.str_print(), static_cast<int>(refresh));
		return NULL;
	}
	{
		statistic_increment(open_tables_count, &LOCK_status);
	}
	table->reginfo.lock_type = need_write ? TL_WRITE : TL_READ;
	table->use_all_columns();
	return table;
}

static TDHS_INLINE int tdhs_open_index(tdhs_table_t& tdhs_table,
		const tdhs_request_table_t &request_table) {
	THD * thd = tdhs_table.thd;
	TABLE *table = tdhs_table.table;
	size_t idxnum = static_cast<size_t>(-1);
	if (request_table.index.str[0] >= '0'
			&& request_table.index.str[0] <= '9') {
		size_t index_num = atoi(request_table.index.str);
		if (index_num < table->s->keys) {
			idxnum = index_num;
		}
	} else if (request_table.index.str[0] == '|') {
		//兼容字段描述
		tdhs_string_t index_fields[REQUEST_MAX_KEY_NUM];
		size_t index_size = 0;
		size_t pos = 0;
		const char * tmp;
		size_t tmp_size = 0;
		while (pos < request_table.index.len) {
			if (*(request_table.index.str + pos) == '|'
					|| *(request_table.index.str + pos) == 0) {
				if (tmp_size > 0) {
					tdhs_string_t& now_s = index_fields[index_size++];
					now_s.str = tmp;
					now_s.len = tmp_size;
					if (index_size >= REQUEST_MAX_KEY_NUM) {
						break;
					}
				}
				tmp = request_table.index.str + pos + 1;
				tmp_size = 0;
			} else {
				tmp_size++;
			}
			pos++;
		}

		easy_debug_log("TDHS: find index field [%d]", index_size);
		if (index_size > 0) {
			for (uint i = 0; i < table->s->keys; ++i) {
				KEY& kinfo = table->key_info[i];
				if (index_size <= kinfo.key_parts) {
					size_t k_idx = 0;
					for (; k_idx < index_size; k_idx++) {
						const KEY_PART_INFO &kpt = kinfo.key_part[k_idx];
						const tdhs_string_t &index_field = index_fields[k_idx];
						if (!kpt.field || !kpt.field->field_name
								|| strlen(kpt.field->field_name)
										!= index_field.len
								|| strncasecmp(kpt.field->field_name,
										index_field.str, index_field.len)
										!= 0) {
							break;
						}
					}
					if (k_idx == index_size) {
						idxnum = i;
						break;
					}
				}
			}
		}
	} else {
		for (uint i = 0; i < table->s->keys; ++i) {
			KEY& kinfo = table->key_info[i];
			if (strcasecmp(kinfo.name, request_table.index.str) == 0) {
				idxnum = i;
				break;
			}
		}
	}
	if (idxnum == size_t(-1)) {
		easy_warn_log( "TDHS: failed to open index %p [%s] [%s] [%s]\n",
				thd, request_table.db.str, request_table.table.str, request_table.index.str);
		return EASY_ERROR;
	}
	tdhs_table.idxnum = idxnum;
	return EASY_OK;
}

static TDHS_INLINE void tdhs_close_table(THD *thd) {
	statistic_increment(close_tables_count, &LOCK_status);
	close_thread_tables(thd);
#if MYSQL_VERSION_ID >= 50505
	thd->mdl_context.release_transactional_locks();
#endif
}

static TDHS_INLINE MYSQL_LOCK * tdhs_lock_table(THD *thd, TABLE** tables,
		uint count, bool for_write_flag) {
	MYSQL_LOCK *lock;
	if (!for_write_flag) {
		//防止innodb提升锁
		thd->lex->sql_command = SQLCOM_SELECT;
	}
#if MYSQL_VERSION_ID >= 50505
	lock = thd->lock = mysql_lock_tables(thd, tables, count, 0);
#else
	bool need_reopen = false;
	lock = thd->lock = mysql_lock_tables(thd, tables, count,
			MYSQL_LOCK_NOTIFY_IF_NEED_REOPEN, &need_reopen);
#endif
	statistic_increment(lock_tables_count, &LOCK_status);
	if (lock == NULL) {
		easy_error_log( "TDHS: lock table failed! thd [%p]", thd);
		return NULL;
	}
	if (for_write_flag) {
#if MYSQL_VERSION_ID >= 50505
		thd->set_current_stmt_binlog_format_row();
#else
		thd->current_stmt_binlog_row_based = 1;
#endif
	}

	easy_debug_log("TDHS: tdhs_lock_table! table count[%d]", count);
	return lock;
}

static TDHS_INLINE int tdhs_unlock_table(THD *thd, MYSQL_LOCK** lock,
		easy_hash_t* opened_table, bool for_write_flag, int write_error) {
	int ret = EASY_OK;
	easy_debug_log("TDHS:tdhs_unlock_table! lock [%p]", *lock);
	if (*lock != NULL) {
		if (for_write_flag && opened_table->count > 0) {
			opened_table_t * t;
			TABLE* mt;
			uint32_t i = 0;
			easy_hash_list_t * node;
			easy_hash_for_each(i, node, opened_table) {
				t = (opened_table_t*) ((char*) node - opened_table->offset);
				if (t->modified && t->mysql_table != NULL) {
					mt = (TABLE*) t->mysql_table;
					query_cache_invalidate3(thd, mt, 1);
					mt->file->ha_release_auto_increment();
				}
			}
		}
		{
			bool suc = true;
#if MYSQL_VERSION_ID >= 50505
			if (write_error) {
				suc = trans_rollback_stmt(thd);
				tb_assert(suc == false);
			} else {
				suc = (trans_commit_stmt(thd) == FALSE);
			}
#else
			suc = (ha_autocommit_or_rollback(thd, write_error) == 0);
#endif
			if (!suc) {
				easy_warn_log(
						"TDHS: commit failed, it's rollback! thd [%p] write_error [%d] ",
						thd, write_error);
				ret = EASY_ERROR;
			}
		}
		mysql_unlock_tables(thd, *lock);
		*lock = thd->lock = NULL;
		statistic_increment(unlock_tables_count, &LOCK_status);
	}
	return ret;
}

static TDHS_INLINE int tdhs_unlock_table_lite(THD *thd, MYSQL_LOCK** lock,
		opened_table_t** opened_table, size_t opend_table_size,
		bool for_write_flag, int write_error) {
	int ret = EASY_OK;
	easy_debug_log("TDHS:tdhs_unlock_table! lock [%p]", *lock);
	if (*lock != NULL) {
		if (for_write_flag && opend_table_size > 0) {
			opened_table_t * t;
			TABLE* mt;
			for (size_t i = 0; i < opend_table_size; i++) {
				t = opened_table[i];
				if (t != NULL && t->modified && t->mysql_table != NULL) {
					mt = (TABLE*) t->mysql_table;
					query_cache_invalidate3(thd, mt, 1);
					mt->file->ha_release_auto_increment();
				}
			}
		}
		{
			bool suc = true;
#if MYSQL_VERSION_ID >= 50505
			if (write_error) {
				suc = trans_rollback_stmt(thd);
				tb_assert(suc == false);
			} else {
				suc = (trans_commit_stmt(thd) == FALSE);
			}
#else
			suc = (ha_autocommit_or_rollback(thd, write_error) == 0);
#endif
			if (!suc) {
				easy_warn_log(
						"TDHS: commit failed, it's rollback! thd [%p] write_error [%d] ",
						thd, write_error);
				ret = EASY_ERROR;
			}
		}
		mysql_unlock_tables(thd, *lock);
		*lock = thd->lock = NULL;
		statistic_increment(unlock_tables_count, &LOCK_status);
	}
	return ret;
}

static TDHS_INLINE size_t prepare_keybuf(const tdhs_string_t* keys,
		const uint32_t key_num, uchar *key_buf, TABLE *table, KEY& kinfo) {
	size_t key_len_sum = 0;
	for (size_t i = 0; i < key_num; i++) {
		const KEY_PART_INFO & kpt = kinfo.key_part[i];
		const tdhs_string_t& key = keys[i];
		if (key.len == 0) {
			kpt.field->set_null();
		} else {
			kpt.field->set_notnull();
			kpt.field->store(key.str, key.strlen(), &my_charset_bin);
		}
		key_len_sum += kpt.store_length;
		easy_debug_log("TDHS:key l=%u sl=%zu", kpt.length, kpt.store_length)
	}
	key_copy(key_buf, table->record[0], &kinfo, key_len_sum);
	easy_debug_log("TDHS:keys sum=%zu flen=%u", key_len_sum, kinfo.key_length)
	return key_len_sum;
}

static TDHS_INLINE void dump_record(tdhs_table_t& tdhs_table) {
	char str_buf[MAX_FIELD_WIDTH];
	String rwpstr(str_buf, sizeof(str_buf), &my_charset_bin);
	TABLE *table = tdhs_table.table;
	for (size_t i = 0; i < tdhs_table.field_num; ++i) {
		Field* const field = table->field[tdhs_table.field_idx[i]];
		if (field->is_null()) {
			/* null */
			fprintf(stderr, "-------------------------------------\n[NULL]\n");
		} else {
			field->val_str(&rwpstr, &rwpstr);
			fprintf(stderr, "-------------------------------------\n"
					"content:[%s]\n"
					"type:[%d]\n"
					"result_type:[%d]\n"
					"real_length:[%d]\n"
					"str_length:[%d]\n", rwpstr.c_ptr(), field->type(),
					field->result_type(), field->data_length(),
					rwpstr.length());
		}
	}
	fprintf(stderr, "#################################\n");
}

static TDHS_INLINE void dump_field(Field* const field) {
	char str_buf[MAX_FIELD_WIDTH];
	String rwpstr(str_buf, sizeof(str_buf), &my_charset_bin);
	if (field->is_null()) {
		/* null */
		fprintf(stderr, "-------------------------------------\n[NULL]\n");
	} else {
		field->val_str(&rwpstr, &rwpstr);
		fprintf(stderr, "-------------------------------------\n"
				"content:[%s]\n"
				"type:[%d]\n"
				"result_type:[%d]\n"
				"real_length:[%d]\n"
				"str_length:[%d]\n", rwpstr.c_ptr(), field->type(),
				field->result_type(), field->data_length(), rwpstr.length());
	}
}

//返回true 符合 false 为不符合
static TDHS_INLINE int filter_record(tdhs_table_t &tdhs_table,
		tdhs_filter_t &filter, uchar *filter_buf) {
	easy_debug_log("TDHS:filter_record:################");
	size_t pos = 0;
	TABLE *table = tdhs_table.table;
	for (size_t i = 0; i < filter.filter_num; ++i) {
		easy_debug_log("TDHS:---------------------------");
		Field* const field = table->field[filter.field_idx[i]];
		const size_t packlen = field->pack_length();
		const uchar * const bval = filter_buf + pos;
		int cv = 0;
		if (tdhs_log_level >= EASY_LOG_DEBUG) {
			dump_field(field);
		}
		if (field->is_null()) {
			cv = (filter.value[i].len == 0) ? 0 : -1;
		} else {
			cv = (filter.value[i].len == 0) ? 1 : field->cmp(bval);
		}
		{
			easy_debug_log( "TDHS:filter_record value is [%d][%s] cv is [%d]",
					filter.value[i].len, filter.value[i].str_print(), cv);
		}
		bool cond = true;
		switch (filter.flag[i]) {
		case TDHS_FILTER_EQ:
			cond = (cv == 0);
			break;
		case TDHS_FILTER_GE:
			cond = (cv >= 0);
			break;
		case TDHS_FILTER_LE:
			cond = (cv <= 0);
			break;
		case TDHS_FILTER_GT:
			cond = (cv > 0);
			break;
		case TDHS_FILTER_LT:
			cond = (cv < 0);
			break;
		case TDHS_FILTER_NOT:
			cond = (cv != 0);
			break;
		default:
			//can't be happeded ,because it vailded in decode
			tb_assert(FALSE);
			break;
		}
		if (!cond) {
			easy_debug_log("TDHS:filter_record end:################");
			return false;
		}
		if (filter.value[i].len != 0) {
			pos += packlen;
		}
	}
	easy_debug_log("TDHS:filter_record end:################");
	return true;
}

static TDHS_INLINE int create_response_for_data(tdhs_table_t& tdhs_table,
		easy_request_t *req) {
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	return write_data_header_to_response(*response, tdhs_table.field_num,
			tdhs_table.field_type);
}

static TDHS_INLINE int response_record(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error) {
	char str_buf[MAX_FIELD_WIDTH];
	String rwpstr(str_buf, sizeof(str_buf), &my_charset_bin);
	TABLE *table = tdhs_table.table;
	for (size_t i = 0; i < tdhs_table.field_num; ++i) {
		Field* const field = table->field[tdhs_table.field_idx[i]];
		if (field->is_null()) {
			if (write_data_to_response(req, 0, 0, client_wait) != EASY_OK) {
				return EASY_ASYNC;
			}
		} else {
			field->val_str(&rwpstr, &rwpstr);
			if (rwpstr.length() != 0) {
				if (write_data_to_response(req, rwpstr.length(), rwpstr.ptr(),
						client_wait) != EASY_OK) {
					return EASY_ASYNC;
				}
			} else {
				static const char empty_str = '\0';
				if (write_data_to_response(req, 1, &empty_str,
						client_wait)!=EASY_OK) {
					return EASY_ASYNC;
				}
			}
		}
	}
	return EASY_OK;
}

static TDHS_INLINE int save_now(THD *thd, Field *field) {
	Item_func_now_local now;
	if (now.fix_fields(thd, NULL) != FALSE) {
		return -1;
	}
	return now.save_in_field(field, 0);
}

static TDHS_INLINE int create_response_for_update(tdhs_table_t& tdhs_table,
		easy_request_t *req) {
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	return write_update_header_to_response(*response, MYSQL_TYPE_LONG, 10, 2);
}

static TDHS_INLINE int create_response_for_count(tdhs_table_t& tdhs_table,
		easy_request_t *req) {
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	return write_update_header_to_response(*response, MYSQL_TYPE_LONG, 10, 1);
}

#define FIELD_STORE_ERROR 198

static TDHS_INLINE int update_record(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error) {
	tdhs_packet_t *packet = (tdhs_packet_t*) ((req->ipacket));
	tdhs_request_t &request = packet->req;
	TABLE * const table = tdhs_table.table;
	handler * const hnd = table->file;
	uchar * const buf = table->record[0];
	store_record(table, record[1]);
	for (size_t i = 0; i < tdhs_table.field_num; i++) {
		long long nval = 0;
		long long pval = 0;
		long long llv = 0;

		Field * const fld = table->field[tdhs_table.field_idx[i]];
		tdhs_string_t &value = request.values.value[i];
		switch (request.values.flag[i]) {
		case TDHS_UPDATE_SET:
			if (value.len == 0) {
				fld->set_null();
			} else {
				fld->set_notnull();
				if (fld->store(value.str, value.strlen(), &my_charset_bin)
						< 0) {
					easy_error_log("TDHS:update store error!");
					*process_db_ret = FIELD_STORE_ERROR;
//					*write_error = 1;
					return EASY_OK;
				}
			}
			break;
		case TDHS_UPDATE_ADD:
			pval = fld->val_int();
			llv = value.len == 0 ? 0 : atoll(value.str);
			nval = pval + llv;
			if (fld->store(nval, false) < 0) {
				easy_error_log("TDHS:update store error!");
				*process_db_ret = FIELD_STORE_ERROR;
//				*write_error = 1;
				return EASY_OK;
			}
			break;
		case TDHS_UPDATE_SUB:
			pval = fld->val_int();
			llv = value.len == 0 ? 0 : atoll(value.str);
			nval = pval - llv;
			if (fld->store(nval, false) < 0) {
				easy_error_log("TDHS:update store error!");
				*process_db_ret = FIELD_STORE_ERROR;
//				*write_error = 1;
				return EASY_OK;
			}
			break;
		case TDHS_UPDATE_NOW:
			if (save_now(tdhs_table.thd, fld) < 0) {
				easy_error_log("TDHS:update store error!");
				*process_db_ret = FIELD_STORE_ERROR;
//				*write_error = 1;
				return EASY_OK;
			}
			break;
		default:
			tb_assert(false);
			break;
		}
	}
	int ret = hnd->ha_update_row(table->record[1], buf);
	if (ret == 0) {
		tdhs_table.change_row_num++;
		tdhs_table.update_row_num++;
		request.opened_table->modified = true;
	} else if (ret == HA_ERR_RECORD_IS_THE_SAME) {
		tdhs_table.update_row_num++;
	} else {
		*process_db_ret = ret;
//		*write_error = 1;
	}
	return EASY_OK;
}

static TDHS_INLINE int count_record(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error) {
	tdhs_table.change_row_num++;
	return EASY_OK;
}

static TDHS_INLINE int delete_record(tdhs_table_t& tdhs_table,
		easy_request_t *req, tdhs_client_wait_t &client_wait,
		int *process_db_ret, int *write_error) {
	tdhs_packet_t *packet = (tdhs_packet_t*) ((req->ipacket));
	tdhs_request_t &request = packet->req;
	TABLE * const table = tdhs_table.table;
	handler * const hnd = table->file;
	int ret = hnd->ha_delete_row(table->record[0]);
	if (ret == 0) {
		tdhs_table.change_row_num++;
		tdhs_table.update_row_num++;
		request.opened_table->modified = true;
	} else {
		*process_db_ret = ret;
//		*write_error = 1;
	}
	return EASY_OK;
}

static TDHS_INLINE void end_update_record(tdhs_table_t& tdhs_table,
		easy_request_t *req) {
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	write_update_ender_to_response(*response, tdhs_table.update_row_num,
			tdhs_table.change_row_num);
}

static TDHS_INLINE void end_count_record(tdhs_table_t& tdhs_table,
		easy_request_t *req) {
	tdhs_packet_t *response = (tdhs_packet_t*) ((req->opacket));
	write_count_ender_to_response(*response, tdhs_table.change_row_num);
}

static TDHS_INLINE int index_prev_same(handler * const hnd, TABLE *table,
		const uchar *key, uint keylen) {
	int r;
	if (!(r = hnd->index_prev(table->record[0]))) {
		if (key_cmp_if_same(table, key, hnd->active_index, keylen)) {
			r = HA_ERR_END_OF_FILE;
		}
	}
	return r;
}

} // namespace taobao

#endif /* TDH_SOCKET_DBUTIL_HPP_ */
