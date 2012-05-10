/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_error.hpp
 *
 *  Created on: 2011-10-25
 *      Author: wentong
 */

#ifndef TDH_SOCKET_ERROR_HPP_
#define TDH_SOCKET_ERROR_HPP_

namespace taobao {

typedef enum {
	CLIENT_ERROR_CODE_FAILED_TO_OPEN_TABLE = 1,
	CLIENT_ERROR_CODE_FAILED_TO_OPEN_INDEX,
	CLIENT_ERROR_CODE_FAILED_TO_MISSING_FIELD,
	CLIENT_ERROR_CODE_FAILED_TO_MATCH_KEY_NUM,
	CLIENT_ERROR_CODE_FAILED_TO_LOCK_TABLE,
	CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY,
	CLIENT_ERROR_CODE_DECODE_REQUEST_FAILED,
	CLIENT_ERROR_CODE_FAILED_TO_MISSING_FIELD_IN_FILTER_OR_USE_BLOB,
	CLIENT_ERROR_CODE_FAILED_TO_COMMIT,
	CLIENT_ERROR_CODE_NOT_IMPLEMENTED,
	CLIENT_ERROR_CODE_REQUEST_TIME_OUT,
	CLIENT_ERROR_CODE_UNAUTHENTICATION,
	CLIENT_ERROR_CODE_KILLED,
	CLIENT_ERROR_CODE_THROTTLED,
	CLIENT_ERROR_CODE_END
} tdhs_client_error_code_t;

extern const char* error_msg[];

} // namespace taobao

#endif /* TDH_SOCKET_ERROR_HPP_ */
