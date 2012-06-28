/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_encode_response.hpp
 *
 *  Created on: 2011-10-25
 *      Author: wentong
 */

#ifndef TDH_SOCKET_ENCODE_RESPONSE_HPP_
#define TDH_SOCKET_ENCODE_RESPONSE_HPP_

#include "tdh_socket_config.hpp"
#include "tdh_socket_protocol.hpp"
#include "tdh_socket_error.hpp"
#include "tdh_socket_dbcontext.hpp"
#include "util.hpp"
#include "debug_util.hpp"
#include "thread_and_lock.hpp"
#include "easy_io.h"

#include  <arpa/inet.h>

namespace taobao {

static TDHS_INLINE int tdhs_response_batch(tdhs_packet_t *packet);

static TDHS_INLINE int tdhs_response_error(tdhs_packet_t *packet,
		tdhs_client_status_t status, uint32_t code);

static TDHS_INLINE int tdhs_response_error_global(tdhs_packet_t *packet,
		tdhs_client_status_t status, uint32_t code);

static TDHS_INLINE void tdhs_init_response(tdhs_packet_t *packet);

static TDHS_INLINE int write_data_header_to_response(tdhs_packet_t& packet,
		uint32_t field_num, char* field_types);

static TDHS_INLINE int write_update_header_to_response(tdhs_packet_t& packet,
		char type, size_t field_size, size_t num);

static TDHS_INLINE void write_update_ender_to_response(tdhs_packet_t& packet,
		uint32_t update_row_num, uint32_t change_row_num);

static TDHS_INLINE void write_count_ender_to_response(tdhs_packet_t& packet,
		uint32_t count);

static TDHS_INLINE void write_insert_ender_to_response(tdhs_packet_t& packet,
		unsigned long long int key);

static TDHS_INLINE int send_stream(tdhs_client_wait_t &client_wait,
		tdhs_packet_t &packet, easy_request_t *req, easy_buf_t *b);

static TDHS_INLINE int write_data_to_response(easy_request_t *req,
		uint32_t data_length, const char* data,
		tdhs_client_wait_t &client_wait);

static TDHS_INLINE int tdhs_response_batch(tdhs_packet_t *packet) {
	packet->command_id_or_response_code = CLIENT_STATUS_MULTI_STATUS;
	packet->length = 0; //这个包的length是0
	easy_pool_t *pool = packet->pool;
	tb_assert(pool!=NULL);
	easy_buf_t *b = easy_buf_create(pool, TDH_SOCKET_HEADER_LENGTH);
	if (b == NULL) {
		easy_error_log("TDHS:tdhs_response_batch create packet buf failed!");
		return EASY_ERROR;
	}
	b->last += TDH_SOCKET_HEADER_LENGTH; //留出空间给header
	packet->wbuff = b;
	return EASY_OK;
}

static TDHS_INLINE int tdhs_response_error(tdhs_packet_t *packet,
		tdhs_client_status_t status, uint32_t code) {
	uint32_t length = sizeof(uint32_t);
	packet->command_id_or_response_code = status;
	packet->length = length;
	easy_pool_t *pool = packet->pool;
	tb_assert(pool!=NULL);
	easy_buf_t *b = easy_buf_create(pool, length + TDH_SOCKET_HEADER_LENGTH);
	if (b == NULL) {
		easy_error_log("TDHS:tdhs_response_error create packet buf failed!");
		return EASY_ERROR;
	}
	b->last += TDH_SOCKET_HEADER_LENGTH; //留出空间给header
	packet->wbuff = b;
	*((uint32_t*) (b->last)) = htonl(code);
	b->last += length;
	return EASY_OK;
}

static TDHS_INLINE int tdhs_response_error_global(tdhs_packet_t *packet,
		tdhs_client_status_t status, uint32_t code) {
	packet->next = NULL; //全局类的返回错误需要断开链  以使batch的返回不会出现连请求也返回回去的情况
	return tdhs_response_error(packet, status, code);
}

static TDHS_INLINE void tdhs_init_response(tdhs_packet_t *packet) {
	packet->command_id_or_response_code = CLIENT_STATUS_OK;
	packet->length = 0;
}

static TDHS_INLINE int write_data_header_to_response(tdhs_packet_t& packet,
		uint32_t field_num, char* field_types) {
	uint32_t size = sizeof(uint32_t) + field_num;
	uint32_t alloc_size =
			size > tdhs_write_buff_size ? size : tdhs_write_buff_size;
	easy_pool_t *pool = packet.pool;
	tb_assert(pool!=NULL);
	easy_buf_t *b = easy_buf_create(pool,
			alloc_size + TDH_SOCKET_HEADER_LENGTH);
	if (b == NULL) {
		easy_error_log(
				"TDHS:write_data_header_to_response create packet buf failed!");
		return EASY_ERROR;
	}
	b->last += TDH_SOCKET_HEADER_LENGTH; //留出空间给header
	packet.stream_buffer = b->pos;
	packet.wbuff = b;
	*((uint32_t*) (b->last)) = htonl(field_num);
	b->last += sizeof(uint32_t);
	for (uint32_t i = 0; i < field_num; i++) {
		*((char*) b->last) = field_types[i];
		b->last++;
	}
	packet.length += size;
	return EASY_OK;
}

static TDHS_INLINE int write_update_header_to_response(tdhs_packet_t& packet,
		char type, size_t field_size, size_t num) {
	uint32_t size = sizeof(uint32_t) + num;
	uint32_t alloc_size = size + (sizeof(uint32_t) + field_size) * num;
	easy_pool_t *pool = packet.pool;
	tb_assert(pool!=NULL);
	easy_buf_t *b = easy_buf_create(pool,
			alloc_size + TDH_SOCKET_HEADER_LENGTH);
	if (b == NULL) {
		easy_error_log(
				"TDHS:write_data_header_to_response create packet buf failed!");
		return EASY_ERROR;
	}
	b->last += TDH_SOCKET_HEADER_LENGTH; //留出空间给header
	packet.stream_buffer = b->pos;
	packet.wbuff = b;
	*((uint32_t*) (b->last)) = htonl(num); //放两个字段  update_row_num 和 change_row_num
	b->last += sizeof(uint32_t);

	for (size_t i = 0; i < num; i++) {
		*((char*) b->last) = type;
		b->last++;
	}

	packet.length += size;
	return EASY_OK;
}

static TDHS_INLINE void write_update_ender_to_response(tdhs_packet_t& packet,
		uint32_t update_row_num, uint32_t change_row_num) {
	char value[11];
	size_t write_len = 0;
	easy_buf_t *b = packet.wbuff;
	tb_assert(b!=NULL);

	write_len = snprintf(value, 11, "%d", update_row_num);
	*((uint32_t*) (b->last)) = htonl(write_len);
	b->last += sizeof(uint32_t);
	packet.length += sizeof(uint32_t);

	memcpy(b->last, value, write_len);
	b->last += write_len;
	packet.length += write_len;

	write_len = snprintf(value, 11, "%d", change_row_num);
	*((uint32_t*) (b->last)) = htonl(write_len);
	b->last += sizeof(uint32_t);
	packet.length += sizeof(uint32_t);

	memcpy(b->last, value, write_len);
	b->last += write_len;
	packet.length += write_len;
}

static TDHS_INLINE void write_count_ender_to_response(tdhs_packet_t& packet,
		uint32_t count) {
	char value[11];
	size_t write_len = 0;
	easy_buf_t *b = packet.wbuff;
	tb_assert(b!=NULL);

	write_len = snprintf(value, 11, "%d", count);
	*((uint32_t*) (b->last)) = htonl(write_len);
	b->last += sizeof(uint32_t);
	packet.length += sizeof(uint32_t);

	memcpy(b->last, value, write_len);
	b->last += write_len;
	packet.length += write_len;
}

static TDHS_INLINE void write_insert_ender_to_response(tdhs_packet_t& packet,
		unsigned long long int key) {
	char value[48];
	size_t write_len = 0;
	easy_buf_t *b = packet.wbuff;
	tb_assert(b!=NULL);

	write_len = snprintf(value, 48, "%llu", key);
	*((uint32_t*) (b->last)) = htonl(write_len);
	b->last += sizeof(uint32_t);
	packet.length += sizeof(uint32_t);

	memcpy(b->last, value, write_len);
	b->last += write_len;
	packet.length += write_len;
}

static TDHS_INLINE int send_stream(tdhs_client_wait_t &client_wait,
		tdhs_packet_t &packet, easy_request_t *req, easy_buf_t *b) {
	// make sure data_len can be write
	//wait
	if (!client_wait.is_inited) {
		easy_debug_log("TDHS:tdhs_client_wait_t init!");
		easy_client_wait_init(&client_wait.client_wait);
		client_wait.is_inited = true;
		req->args = &client_wait;
	}
	packet.command_id_or_response_code = CLIENT_STATUS_ACCEPT;

	pthread_mutex_lock(&client_wait.client_wait.mutex);
	easy_request_wakeup(req);
	((tdhs_dbcontext_i *) client_wait.db_context)->using_stream();
	client_wait.is_waiting = true;
	pthread_cond_wait(&client_wait.client_wait.cond,
			&client_wait.client_wait.mutex);
	client_wait.is_waiting = false;
	pthread_mutex_unlock(&client_wait.client_wait.mutex);
	if (client_wait.is_closed) {
		return EASY_ERROR;
	}

	req->opacket = req->ipacket; //重新指定response
	tdhs_init_response(&packet); //重新初始化packet
	easy_list_init(&b->node);
	b->pos = packet.stream_buffer; //复位buf
	b->last = b->pos + TDH_SOCKET_HEADER_LENGTH; //复位buf,并预留空间给header
	return EASY_OK;
}

static TDHS_INLINE int write_data_to_response(easy_request_t *req,
		uint32_t data_length, const char* data,
		tdhs_client_wait_t &client_wait) {
	tdhs_packet_t &packet = *((tdhs_packet_t*) (req->ipacket));
	easy_buf_t *b = packet.wbuff;
	tb_assert(b!=NULL);
	if ((uint32_t) (b->end - b->last) < sizeof(uint32_t)) { // make sure data_len can be write
		if (send_stream(client_wait, packet, req, b) != EASY_OK) {
			return EASY_ERROR;
		}
	}
	*((uint32_t*) (b->last)) = htonl(data_length);
	b->last += sizeof(uint32_t);
	packet.length += sizeof(uint32_t);
	if (data_length > 0) {
		uint32_t read_pos = 0;
		while (true) {
			uint32_t write_len =
					min((uint32_t) (b->end - b->last),(data_length - read_pos));
			easy_debug_log("TDHS:send_stream write_len[%d] ", write_len);
			easy_debug_log("TDHS:buf info pos[%p] last[%p] end[%p]",
					b->pos, b->last, b->end);
			if (write_len > 0) {
				memcpy(b->last, data + read_pos, write_len);
				b->last += write_len;
				packet.length += write_len;
				read_pos += write_len;
			}
			if (read_pos < data_length) {
				easy_debug_log("TDHS:send_stream read_pos[%d] data_len[%d]",
						read_pos, data_length);
				if (send_stream(client_wait, packet, req, b) != EASY_OK) {
					return EASY_ERROR;
				}
			} else {
				easy_debug_log(
						"TDHS:send_stream_done read_pos[%d] data_len[%d]",
						read_pos, data_length);
				break;
			}
		}
	}
	return EASY_OK;
}

} // namespace taobao

#endif /* TDH_SOCKET_ENCODE_RESPONSE_HPP_ */
