/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_table_balance.hpp
 *
 *  Created on: 2011-12-9
 *      Author: wentong
 */

#ifndef TDH_SOCKET_TABLE_BALANCE_HPP_
#define TDH_SOCKET_TABLE_BALANCE_HPP_

#include "tdh_socket_define.hpp"

namespace taobao {
//返回需要分配的hash值
extern uint64_t table_need_balance(tdhs_request_t& req, tdhs_optimize_t type,
		uint32_t custom_hash);

} // namespace taobao

#endif /* TDH_SOCKET_TABLE_BALANCE_HPP_ */
