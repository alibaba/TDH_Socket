/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_connection_context.hpp
 *
 *  Created on: 2011-9-29
 *      Author: wentong
 */

#ifndef TDH_SOCKET_CONNECTION_CONTEXT_HPP_
#define TDH_SOCKET_CONNECTION_CONTEXT_HPP_
#include "tdh_socket_protocol.hpp"
#include "tdh_socket_dbcontext.hpp"
#include "util.hpp"
#include <easy_io.h>
#include <easy_log.h>

namespace taobao {

class tdh_socket_connection_context: private noncopyable {
public:
	tdh_socket_connection_context();
	virtual ~tdh_socket_connection_context();
	int init(easy_connection_t* c);
	int destory();
	int decode(tdhs_packet_t* request);
	int shake_hands_if(tdhs_packet_t* request);
	uint32_t get_timeout();
	bool can_read();
	bool can_write();
private:
	bool is_shaked;
	easy_connection_t* connection;
	decode_request_t* decode_request;
	uint32_t time_out;
	char permission;
};

} /* namespace taobao */
#endif /* TDH_SOCKET_CONNECTION_CONTEXT_HPP_ */
