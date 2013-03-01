/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_dbcontext.hpp
 *
 *  Created on: 2011-9-30
 *      Author: wentong
 */

#ifndef TDH_SOCKET_DBCONTEXT_HPP_
#define TDH_SOCKET_DBCONTEXT_HPP_

#include "tdh_socket_protocol.hpp"
#include "tdh_socket_define.hpp"
#include <stdint.h>
#include <easy_log.h>
#include <easy_pool.h>
#include <easy_io.h>

namespace taobao {

extern const char* PRIMARY;

#define PRIMARY_STRING  "PRIMARY"

#define PRIMARY_SIZE (sizeof(PRIMARY_STRING))

typedef int (decode_request_t)(tdhs_request_t &req, tdhs_packet_t& packet);

extern decode_request_t* decode_request_array[TDHS_PROTOCOL_END];

class tdh_socket_connection_context;
class tdhs_dbcontext_i {
public:
	virtual ~tdhs_dbcontext_i() {
	}
	virtual int init(tdhs_optimize_t _type, const void *stack_bottom) = 0;
	virtual tdhs_optimize_t get_type() = 0;
	virtual unsigned long get_use_steam_count() = 0;
	virtual bool need_write() = 0;
	virtual void open_table(tdhs_request_t &req) = 0;
	virtual void lock_table() = 0;
	virtual int unlock_table() = 0;
	virtual void close_table() = 0;
	virtual void close_cached_table() = 0;
	virtual int destory() = 0;
	virtual int execute(easy_request_t *r) = 0;
	virtual void set_thd_info(unsigned int bulk_request_num) = 0;
	virtual void using_stream() = 0;
	virtual void set_group_commit(bool gc) = 0;
	virtual time_t* get_thd_time() = 0;

    virtual bool need_close_table() = 0;
    virtual void set_need_close_table(bool need) = 0;
#ifdef TDHS_ROW_CACHE
	virtual bool is_in_cache(tdhs_request_t &req) = 0;
#endif
	static tdhs_dbcontext_i* create();
};

}
// namespace taobao

#endif /* TDH_SOCKET_DBCONTEXT_HPP_ */
