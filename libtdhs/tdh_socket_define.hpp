/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_define.hpp
 *
 *  Created on: 2011-10-26
 *      Author: wentong
 */

#ifndef TDH_SOCKET_DEFINE_HPP_
#define TDH_SOCKET_DEFINE_HPP_

#include "easy_log.h"
#include "easy_io.h"
#include "easy_hash.h"
#include "thread_and_lock.hpp"
#include "debug_util.hpp"

namespace taobao {

typedef struct tdhs_string_t tdhs_string_t;
typedef struct tdhs_request_table_t tdhs_request_table_t;
typedef struct tdhs_filter_t tdhs_filter_t;
typedef struct tdhs_get_key_t tdhs_get_key_t;
typedef struct tdhs_request_get_t tdhs_request_get_t;
typedef struct tdhs_request_values_t tdhs_request_values_t;
typedef struct tdhs_request_insert_t tdhs_request_insert_t;
typedef struct opened_table_t opened_table_t;
typedef struct tdhs_request_t tdhs_request_t;
typedef struct tdhs_client_wait_t tdhs_client_wait_t;

static TDHS_INLINE uint64_t fold(uint64_t n1, uint64_t n2);

static TDHS_INLINE uint64_t make_hash_code_for_table(const char* db,
		size_t db_len, const char* table, size_t table_len);

typedef enum {
	//read
	REQUEST_TYPE_GET = 0,
	REQUEST_TYPE_COUNT = 1,
	//write
	REQUEST_TYPE_UPDATE = 10,
	REQUEST_TYPE_DELETE = 11,
	REQUEST_TYPE_INSERT = 12,
	//extra
	REQUEST_TYPE_BATCH = 20,
	REQUEST_TYPE_END,
	REQUEST_TYPE_SHAKE_HANDS = 0xFFFF
} tdhs_request_type_t;

typedef enum {
	//正确返回status
	CLIENT_STATUS_OK = 200, //完成所有数据的返回
	CLIENT_STATUS_ACCEPT = 202, //对于流的处理,还有未返回的数据
	CLIENT_STATUS_MULTI_STATUS = 207, //对于batch请求的返回,表示后面跟了多个请求
	//请求导致的错误信息
	CLIENT_STATUS_BAD_REQUEST = 400,
	CLIENT_STATUS_FORBIDDEN = 403, //没权限
	CLIENT_STATUS_NOT_FOUND = 404, //没有找到资源,如 db/table/index 等
	CLIENT_STATUS_REQUEST_TIME_OUT = 408, //超时
	//服务器导致的错误信息
	CLIENT_STATUS_SERVER_ERROR = 500, //server无法处理的错误,比如内存不够
	CLIENT_STATUS_NOT_IMPLEMENTED = 501, //server没实现这个功能
	CLIENT_STATUS_DB_ERROR = 502, //handler返回的错误信息
	CLIENT_STATUS_SERVICE_UNAVAILABLE = 503
//被kill这种情况或被流控
//负载过重
} tdhs_client_status_t;

#define UT_HASH_RANDOM_MASK	1463735687
#define UT_HASH_RANDOM_MASK2	1653893711
static TDHS_INLINE uint64_t fold(uint64_t n1, uint64_t n2) {
	return (((((n1 ^ n2 ^ UT_HASH_RANDOM_MASK2) << 8) + n1)
			^ UT_HASH_RANDOM_MASK) + n2);
}

#define all_hash_code_for_table(db, db_str_len, table, table_str_len) \
	fold(easy_hash_code((db), (db_str_len), 5), easy_hash_code((table), (table_str_len), 5))

#define is_number(C) ((C)>='0'&&(C)<='9')

static TDHS_INLINE uint64_t make_hash_code_for_table(const char* db,
		size_t db_len, const char* table, size_t table_len) {
	size_t i = table_len - 1;
	//如果table name最后是以数字结尾的话，就以这个数字为hash code
	if (is_number(table[i])) {
		for (;;) {
			if (!is_number(table[i])) {
				i++;
				break;
			} else if (i == 0) {
				break;
			} else {
				i--;
			}
		}
		uint64_t ret = atoll(table + i);
		ret = (ret == 0 ? ~0 : ret);
		return ret;
	} else {
		return all_hash_code_for_table(db,db_len,table,table_len);
	}
}

#define REQUEST_MAX_FIELD_NUM 256
#define REQUEST_MAX_KEY_NUM 10
#define REQUEST_MAX_IN_NUM REQUEST_MAX_KEY_NUM

struct tdhs_string_t {
	const char *str; //C字符串 必须包含'\0'
	size_t len; //包含'\0'的长度

	TDHS_INLINE
	size_t strlen() const { //str长度 不包含'\0'
		return len > 0 ? len - 1 : 0;
	}

	TDHS_INLINE
	const char* str_print() const {
		return len > 0 ? str : "NULL";
	}

	TDHS_INLINE
	int compare(tdhs_string_t &str_t) {
		size_t len1 = len;
		size_t len2 = str_t.len;
		const char* v1 = str;
		const char* v2 = str_t.str;
		size_t n = min(len1,len2);
		size_t i;
		for (i = 0; i < n; i++) {
			char c1 = v1[i];
			char c2 = v2[i];
			if (c1 != c2) {
				return c1 - c2;
			}
		}
		return len1 - len2;
	}
};

typedef enum {
	TDHS_UNDECODE = 0, TDHS_DECODE_DONE, TDHS_DECODE_FAILED
} tdhs_decode_status_t;

struct tdhs_request_table_t {
	uint32_t field_num;
	tdhs_string_t db;
	tdhs_string_t table;
	tdhs_string_t index;
	tdhs_string_t fields[REQUEST_MAX_FIELD_NUM];
	mutable uint64_t hash_code_for_table;

	TDHS_INLINE
	void print_log() const {
		if (easy_log_level >= EASY_LOG_DEBUG) {
			easy_debug_log( "TDHS:db:[%s] table:[%s] index:[%s] field_num:[%d]",
					db.str_print(), table.str_print(), index.str_print(), field_num);
			for (uint32_t i = 0; i < field_num; i++) {
				easy_debug_log("TDHS:    field[%d]:[%s]",
						i, fields[i].str_print());
			}
		}
	}

	TDHS_INLINE
	int is_vaild(bool need_field) const {
		print_log();
		if (db.len == 0 || table.len == 0 || (need_field && field_num == 0)) {
			easy_warn_log(
					"request is invaild, db or table or fields is missing!");
			return EASY_ERROR;
		}
		return EASY_OK;
	}

	TDHS_INLINE
	uint64_t hash_code_table() const {
		if (hash_code_for_table == 0) {
			hash_code_for_table = make_hash_code_for_table(db.str, db.strlen(),
					table.str, table.strlen());
		}
		return hash_code_for_table;
	}
};

typedef enum {
	TDHS_FILTER_EQ = 0, // =
	TDHS_FILTER_GE = 1, // >=
	TDHS_FILTER_LE = 2, // <=
	TDHS_FILTER_GT = 3, // >
	TDHS_FILTER_LT = 4, // <
	TDHS_FILTER_NOT = 5, // !
	TDHS_FILTER_END
} tdhs_filter_flag_t;

struct tdhs_filter_t {
	uint32_t filter_num;
	tdhs_string_t field[REQUEST_MAX_FIELD_NUM];
	size_t field_idx[REQUEST_MAX_FIELD_NUM]; //for execute
	tdhs_filter_flag_t flag[REQUEST_MAX_FIELD_NUM];
	tdhs_string_t value[REQUEST_MAX_FIELD_NUM];

	TDHS_INLINE
	int set_flag(uint32_t idx, uint8_t f) {
		if (f < 0 || f >= TDHS_FILTER_END) {
			easy_warn_log( "TDHS:filter flag is out of range [%d]!", f);
			return EASY_ERROR;
		}
		flag[idx] = (tdhs_filter_flag_t) f;
		return EASY_OK;
	}

	TDHS_INLINE
	void print_log() const {
		if (easy_log_level >= EASY_LOG_DEBUG) {
			easy_debug_log("TDHS:filter_num:[%d]", filter_num);
			for (uint32_t i = 0; i < filter_num; i++) {
				easy_debug_log("TDHS:    field[%d]:[%s] flag:[%d] value:[%s]",
						i, field[i].str_print(), flag[i], value[i].str_print());
			}
		}
	}
};

typedef enum {
	TDHS_EQ = 0, // = for asc
	TDHS_GE = 1, // >=
	TDHS_LE = 2, // <=
	TDHS_GT = 3, // >
	TDHS_LT = 4, // <
	TDHS_IN = 5, // in
	TDHS_DEQ = 6, // = for desc
    TDHS_BETWEEN = 7, //between
	TDHS_READ_END
} tdhs_find_flag_t;

struct tdhs_get_key_t {
	uint32_t key_field_num;
	tdhs_string_t key[REQUEST_MAX_KEY_NUM];
};

struct tdhs_request_get_t {
	uint32_t key_num;
	tdhs_get_key_t *keys;
	tdhs_get_key_t r_keys[REQUEST_MAX_IN_NUM];
	tdhs_find_flag_t find_flag;
	tdhs_filter_t filter;
	uint32_t start;
	uint32_t limit;

	TDHS_INLINE
	void print_log() const {
		if (easy_log_level >= EASY_LOG_DEBUG) {
			easy_debug_log(
					"TDHS:find_flag:[%d] start:[%d] limit:[%d],key_num:[%d]",
					find_flag, start, limit, key_num);
			for (uint32_t i = 0; i < key_num; i++) {
				const tdhs_get_key_t &one_key = keys[i];
				for (uint32_t j = 0; j < one_key.key_field_num; j++) {
					easy_debug_log("TDHS:    key[%d][%d]:[%s]",
							i, j, one_key.key[j].str_print());
				}
			}
		}
	}
	TDHS_INLINE
	int is_vaild() const {
		print_log();
		if (find_flag < 0 || find_flag >= TDHS_READ_END || key_num == 0) {
			easy_warn_log(
					"request get is invaild,find_flag is error or key is missing!");
			return EASY_ERROR;
		}
		for (uint32_t i = 0; i < key_num; i++) {
			const tdhs_get_key_t &one_key = keys[i];
			if (one_key.key_field_num == 0) {
				easy_warn_log( "request get is invaild,some key is emtpy!");
				return EASY_ERROR;
			}
		}

		return EASY_OK;
	}
};

typedef enum {
	TDHS_UPDATE_SET = 0,
	TDHS_UPDATE_ADD,
	TDHS_UPDATE_SUB,
	TDHS_UPDATE_NOW,
	TDHS_UPDATE_END
} tdhs_update_flag_t;

struct tdhs_request_values_t {
	uint32_t value_num;
	tdhs_update_flag_t flag[REQUEST_MAX_FIELD_NUM];
	tdhs_string_t value[REQUEST_MAX_FIELD_NUM];

	TDHS_INLINE
	int set_flag(uint32_t idx, uint8_t f) {
		if (f < 0 || f >= TDHS_UPDATE_END) {
			easy_warn_log( "TDHS:update flag is out of range [%d]!", f);
			return EASY_ERROR;
		}
		flag[idx] = (tdhs_update_flag_t) f;
		return EASY_OK;
	}

	TDHS_INLINE
	void print_log() const {
		if (easy_log_level >= EASY_LOG_DEBUG) {
			easy_debug_log("TDHS:value_num:[%d]", value_num);
			for (uint32_t i = 0; i < value_num; i++) {
				easy_debug_log("TDHS:    value[%d]:[%s] flag[%d]",
						i, value[i].str_print(), flag[i]);
			}
		}
	}
};

struct tdhs_request_insert_t {
	uint32_t value_num;
	tdhs_string_t value[REQUEST_MAX_FIELD_NUM];

	TDHS_INLINE
	void print_log() const {
		if (easy_log_level >= EASY_LOG_DEBUG) {
			easy_debug_log("TDHS:value_num:[%d]", value_num);
			for (uint32_t i = 0; i < value_num; i++) {
				easy_debug_log("TDHS:    value[%d]:[%s]",
						i, value[i].str_print());
			}
		}
	}
};

struct opened_table_t {
	void* mysql_table;
	bool modified; //MARK 有记录更新时 需设为true以便更新qurey cache
	easy_hash_list_t hash;
};

struct tdhs_request_t {
	tdhs_decode_status_t status;
	tdhs_request_type_t type;
	tdhs_request_table_t table_info;
	tdhs_request_get_t get;
	tdhs_request_values_t values;
	opened_table_t* opened_table; //放open过得mysql table
	mutable uint64_t optimize_hash; //在optimize中hash过得话 就放着这里缓存
};

typedef enum {
	TDHS_QUICK, TDHS_SLOW, TDHS_WRITE
} tdhs_optimize_t;

struct tdhs_client_wait_t {
	bool is_inited;
	bool is_waiting;
	bool is_closed;
	void *db_context;
	easy_client_wait_t client_wait;
};

} // namespace taobao

#endif /* TDH_SOCKET_DEFINE_HPP_ */
