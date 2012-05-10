/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_error.cpp
 *
 *  Created on: 2011-10-25
 *      Author: wentong
 */

#include "tdh_socket_error.hpp"

namespace taobao {

const char* error_msg[] = { "nothing", "TDH_SOCKET failed to open table!",
		"TDH_SOCKET failed to open index!", "TDH_SOCKET field is missing!",
		"TDH_SOCKET request can't match the key number!",
		"TDH_SOCKET failed to lock table!",
		"TDH_SOCKET server don't have enough memory!",
		"TDH_SOCKET server can't decode your request!",
		"TDH_SOCKET field is missing in filter or use blob!",
		"TDH_SOCKET failed to commit, will be rollback!",
		"TDH_SOCKET not implemented!", "TDH_SOCKET request time out!",
		"TDH_SOCKET request is unauthentication!",
		"TDH_SOCKET request is killed!", "TDH_SOCKET request is throttled!" };

} // namespace taobao

