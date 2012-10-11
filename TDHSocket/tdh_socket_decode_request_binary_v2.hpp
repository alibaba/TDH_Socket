/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_decode_request_binary.hpp
 *
 *  Created on: 2011-11-21
 *      Author: wentong
 */

#ifndef TDH_SOCKET_DECODE_REQUEST_BINARY_V2_HPP_
#define TDH_SOCKET_DECODE_REQUEST_BINARY_V2_HPP_

#include "tdh_socket_dbcontext.hpp"

namespace taobao {

extern int decode_request_by_binary_v2(tdhs_request_t &req, tdhs_packet_t& packet);

} // namespace taobao


#endif /* TDH_SOCKET_DECODE_REQUEST_BINARY_V2_HPP_ */
