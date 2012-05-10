/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_protocol.hpp
 *
 *  Created on: 2011-8-26
 *      Author: wentong
 */

#ifndef TDH_SOCKET_PROTOCOL_HPP_
#define TDH_SOCKET_PROTOCOL_HPP_

#include "tdh_socket_define.hpp"

#include <stdint.h>
#include <easy_define.h>
#include <easy_buf.h>
#include <easy_io.h>

namespace taobao {
//===in shake hander read
#define TDH_SOCKET_SHAKE_HANDS_HEADER {'T','D','H','S'}
#define TDH_SOCKET_SHAKE_HANDS_HEADER_LENGTH 4
#define TDH_SOCKET_PROTOCOL_VERSION_LENGTH sizeof(uint32_t)
#define TDH_SOCKET_TIME_OUT_LENGTH sizeof(uint32_t)
#define TDH_SOCKET_SHAKE_HANDS_TOTAL_SIZE (TDH_SOCKET_SHAKE_HANDS_HEADER_LENGTH + TDH_SOCKET_PROTOCOL_VERSION_LENGTH + TDH_SOCKET_TIME_OUT_LENGTH)

//===in every command read
#define TDHS_MAGIC_CODE (0xFFFFFFFF)
#define TDHS_MAGIC_CODE_SIZE (4)
#define TDH_SOCKET_COMAND_LENGTH sizeof(uint32_t)
#define TDH_SOCKET_SEQ_ID_LENGTH sizeof(uint32_t)
#define TDH_SOCKET_REVERSE_LENGTH sizeof(uint32_t)
#define TDH_SOCKET_SIZE_LENGTH sizeof(uint32_t)
#define TDH_SOCKET_HEADER_LENGTH (TDHS_MAGIC_CODE_SIZE+TDH_SOCKET_COMAND_LENGTH+TDH_SOCKET_SEQ_ID_LENGTH+TDH_SOCKET_REVERSE_LENGTH+TDH_SOCKET_SIZE_LENGTH)

typedef enum {
	TDHS_PROTOCOL_BINARY = 1, TDHS_PROTOCOL_END
} tdhs_protocol_version_t;

typedef struct tdhs_packet_t tdhs_packet_t;

struct tdhs_packet_t {
	uint32_t command_id_or_response_code;
	uint32_t seq_id;
	uint32_t reserved; //当type为REQUEST_TYPE_BATCH,记录批量请求的个数
	uint32_t length;
	char *rdata; //for read
	easy_buf_t *wbuff; //for write
	char *stream_buffer; //for stream reuse
	easy_pool_t *pool;
	tdhs_request_t req;
	unsigned long long int start_time;
	tdhs_packet_t *next; //为batch请求串联用
	char buffer[0];
};

extern void *tdhs_decode(easy_message_t *m);

extern int tdhs_encode(easy_request_t *r, void *data);

extern easy_session_t* create_thds_packet(uint32_t id, char* data,
		uint32_t data_len, int timeout, void* args, bool need_copy = true);

} // namespace taobao

#endif /* TDH_SOCKET_PROTOCOL_HPP_ */
