/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_connection_context.cpp
 *
 *  Created on: 2011-9-29
 *      Author: wentong
 */

#include "tdh_socket_connection_context.hpp"
#include "tdh_socket_encode_response.hpp"
#include "tdh_socket_config.hpp"
#include "tdh_socket_share.hpp"
#include "debug_util.hpp"

#include  <arpa/inet.h>

namespace taobao {

#define PERMISSION_READ 1
#define PERMISSION_WRITE  (1<<1)

tdh_socket_connection_context::tdh_socket_connection_context() {

}

tdh_socket_connection_context::~tdh_socket_connection_context() {
}

int tdh_socket_connection_context::init(easy_connection_t* c) {
	is_shaked = false;
	connection = c;
	decode_request = NULL;
	time_out = 0;
	permission = 0;
	return EASY_OK;
}

int tdh_socket_connection_context::destory() {
	return EASY_OK;
}

int tdh_socket_connection_context::decode(tdhs_packet_t* request) {
	tb_assert(decode_request!=NULL);
	int state = decode_request(request->req, *request);
	if (state) {
		easy_warn_log("TDHS: decode failed!");
		request->req.status = TDHS_DECODE_FAILED;
		if(state == ERROR_OUT_OF_IN)
			return ERROR_OUT_OF_IN;
		else
			return EASY_ERROR;
	}
	request->req.status = TDHS_DECODE_DONE;
	return EASY_OK;
}

int tdh_socket_connection_context::shake_hands_if(tdhs_packet_t *request) {
	if (!is_shaked) {
		char header[TDH_SOCKET_SHAKE_HANDS_HEADER_LENGTH] =
				TDH_SOCKET_SHAKE_HANDS_HEADER;
		if (request->command_id_or_response_code != REQUEST_TYPE_SHAKE_HANDS
				|| request->length < TDH_SOCKET_SHAKE_HANDS_TOTAL_SIZE
				|| strncmp(request->rdata, header,
						TDH_SOCKET_SHAKE_HANDS_HEADER_LENGTH) != 0) {
			char info[TDH_SOCKET_SHAKE_HANDS_TOTAL_SIZE + 1];
			memcpy(info, request->rdata, TDH_SOCKET_SHAKE_HANDS_TOTAL_SIZE);
			info[TDH_SOCKET_SHAKE_HANDS_TOTAL_SIZE] = 0;
			easy_warn_log(
					"TDHS: shake_hands failed! request length is [%d],head is [%s]",
					request->length, info);
			return EASY_ERROR;
		}
		uint32_t protocol_version = ntohl(*((uint32_t*) (request->rdata
								+ TDH_SOCKET_SHAKE_HANDS_HEADER_LENGTH)));
		if (protocol_version >= TDHS_PROTOCOL_END) {
			easy_warn_log("TDHS: shark_hands failed! protocol_version is [%d]",
					protocol_version);
			return EASY_ERROR;
		}
		decode_request = decode_request_array[protocol_version];
		if (decode_request == NULL) {
			easy_warn_log(
					"TDHS: shark_hands failed! can't find decode_request,protocol_version is [%d]",
					protocol_version);
			return EASY_ERROR;
		}
		time_out = ntohl(*((uint32_t*) (request->rdata
								+ TDH_SOCKET_SHAKE_HANDS_HEADER_LENGTH
								+ TDH_SOCKET_PROTOCOL_VERSION_LENGTH)));
		easy_debug_log("TDHS:  timeout is [%d]", time_out);

		//for auth
		char* pos = request->rdata + TDH_SOCKET_SHAKE_HANDS_TOTAL_SIZE;
		uint32_t len = request->length - TDH_SOCKET_SHAKE_HANDS_TOTAL_SIZE;
		tdhs_string_t read_code;
		tdhs_string_t write_code;
		read_uint32_ref(read_code.len, pos, len);
		read_str_ref(read_code.str, pos, len, read_code.len);
		read_uint32_ref(write_code.len, pos, len);
		read_str_ref(write_code.str, pos, len, write_code.len);
		easy_debug_log("TDHS:  read_code [%s] write_code [%s]",
				read_code.str_print(), write_code.str_print());
		if (tdhs_auth_read(read_code)) {
			onBit(permission, PERMISSION_READ);
		}
		if (tdhs_auth_write(write_code)) {
			onBit(permission, PERMISSION_WRITE);
		}
		is_shaked = true;
		return EASY_AGAIN;
	} else {
		return EASY_OK;
	}
}

uint32_t tdh_socket_connection_context::get_timeout() {
	return time_out;
}

bool tdh_socket_connection_context::can_read() {
	if (tdhs_auth_on) {
		return testFlag(permission, PERMISSION_READ);
	}
	return true;
}
bool tdh_socket_connection_context::can_write() {
	if (tdhs_auth_on) {
		return testFlag(permission, PERMISSION_WRITE);
	}
	return true;
}

} /* namespace taobao */
