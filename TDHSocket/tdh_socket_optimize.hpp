/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_optimize.hpp
 *
 *  Created on: 2011-11-10
 *      Author: wentong
 */

#ifndef TDH_SOCKET_OPTIMIZE_HPP_
#define TDH_SOCKET_OPTIMIZE_HPP_

#include "tdh_socket_define.hpp"

namespace taobao {

extern int init_optimize();

extern tdhs_optimize_t optimize(const tdhs_request_t &request);

extern void add_optimize(const tdhs_request_t &request);

extern void destory_optimize();

extern void show_optimize_status(char* status, const size_t n);
} // namespace taobao

#endif /* TDH_SOCKET_OPTIMIZE_HPP_ */
