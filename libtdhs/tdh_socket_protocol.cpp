/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_protocol.cpp
 *
 *  Created on: 2011-9-30
 *      Author: wentong
 */

#include "tdh_socket_protocol.hpp"
#include "debug_util.hpp"
#include "tdh_socket_time.hpp"

#include  <arpa/inet.h>

namespace taobao {

#define GET_HEADER_INFO(COM,SEQ,REV,DATA,POS) \
		COM = ntohl(*((uint32_t *) ((POS) + TDHS_MAGIC_CODE_SIZE)));      \
		SEQ = ntohl(												      \
			*((uint32_t *) ((POS) + TDHS_MAGIC_CODE_SIZE                  \
							+ TDH_SOCKET_COMAND_LENGTH)));                \
		REV =                                                             \
			ntohl(*((uint32_t *) ((POS) + TDHS_MAGIC_CODE_SIZE            \
				+ TDH_SOCKET_COMAND_LENGTH + TDH_SOCKET_SEQ_ID_LENGTH))); \
		DATA =                                                            \
			ntohl( *((uint32_t *) ((POS) + TDHS_MAGIC_CODE_SIZE           \
				+ TDH_SOCKET_COMAND_LENGTH + TDH_SOCKET_SEQ_ID_LENGTH     \
				+ TDH_SOCKET_REVERSE_LENGTH)));                           \

void*tdhs_decode(easy_message_t *m) {
	tdhs_packet_t *packet;
	uint32_t len, magic_code, com_or_resp, seq_id, reverse, datalen;
	uint32_t batch_request_num = 0;
	unsigned long long int now;
	size_t i;
	char *pos; //解析batch请求用
	uint32_t left_length; //判断batch请求长度用

	if ((len = m->input->last - m->input->pos) < TDH_SOCKET_HEADER_LENGTH)
		return NULL;

	// packet
	magic_code = ntohl(*((uint32_t *) (m->input->pos)));
	if (magic_code != TDHS_MAGIC_CODE) {
		easy_error_log("TDHS:magic_code is invalid: %d\n", magic_code);
		m->status = EASY_ERROR;
		return NULL;
	}

	GET_HEADER_INFO(com_or_resp, seq_id, reverse, datalen, m->input->pos)

	batch_request_num = com_or_resp == REQUEST_TYPE_BATCH ? reverse : 0;

	if (datalen > 0x4000000) { // 64M
		easy_error_log("TDHS:data_len is invalid: %d\n", datalen);
		m->status = EASY_ERROR;
		return NULL;
	}

	// 长度不够
	len -= TDH_SOCKET_HEADER_LENGTH;

	if (len < datalen) {
		m->next_read_len = datalen - len;
		return NULL;
	}

	// alloc packet
    if ((packet = (tdhs_packet_t *) easy_pool_alloc(m->pool,
			sizeof(tdhs_packet_t) * (1 + batch_request_num))) == NULL) {
		m->status = EASY_ERROR;
		return NULL;
	}
	now = tdhs_micro_time();

	packet->command_id_or_response_code = com_or_resp;
	packet->seq_id = seq_id;
	packet->reserved = reverse;
	packet->length = datalen;
	m->input->pos += TDH_SOCKET_HEADER_LENGTH;
	packet->rdata = (char *) m->input->pos;
	packet->pool = m->pool; //此处设置使用message的pool
	packet->start_time = now;

    packet->wbuff=0;
    packet->stream_buffer=0;
    packet->next=0;
    memset(&packet->req,0,sizeof(tdhs_request_t));
	//处理batch请求
	pos = packet->rdata;
	left_length = packet->length;
	for (i = 0; i < batch_request_num; i++) {
		tdhs_packet_t *batch_packet = packet + 1 + i;
		if (left_length < TDH_SOCKET_HEADER_LENGTH) {
			//长度不全导致没法完整解析batch请求的头
			easy_error_log( "TDHS:batch left_length is invalid: %d  %d\n",
					left_length);
			m->status = EASY_ERROR;
			return NULL;
		}

		GET_HEADER_INFO(com_or_resp, seq_id, reverse, datalen, pos)
		pos += TDH_SOCKET_HEADER_LENGTH;
		left_length -= TDH_SOCKET_HEADER_LENGTH;
		if (left_length < datalen) {
			//长度不全导致没法完整解析batch请求
			easy_error_log(
					"TDHS:batch data_len is invalid: %d ,the left_length is %d\n",
					datalen, left_length);
			m->status = EASY_ERROR;
			return NULL;
		}
		batch_packet->command_id_or_response_code = com_or_resp;
		batch_packet->seq_id = seq_id;
		//batch 目前不支持嵌套 ,所以不设置reserved
		batch_packet->length = datalen;
		batch_packet->rdata = pos;
		batch_packet->pool = m->pool; //此处设置使用message的pool
		batch_packet->start_time = now;

        batch_packet->wbuff=0;
        batch_packet->stream_buffer=0;
        memset(&batch_packet->req,0,sizeof(tdhs_request_t));

		(batch_packet - 1)->next = batch_packet; //形成链
		pos += datalen;
		left_length -= datalen;
	}
	m->input->pos += packet->length;
	return packet;
}

#define WRITE_HEADER_INFO(COM,SEQ,REV,DATA,POS)                                   \
	*((uint32_t *) (POS)) = htonl(TDHS_MAGIC_CODE);                                \
	*((uint32_t *) (POS + TDHS_MAGIC_CODE_SIZE)) = htonl(COM);                     \
	*((uint32_t *) (POS+ TDHS_MAGIC_CODE_SIZE + TDH_SOCKET_COMAND_LENGTH)) =       \
			htonl(SEQ);                                                            \
	*((uint32_t *) (POS + TDHS_MAGIC_CODE_SIZE + TDH_SOCKET_COMAND_LENGTH          \
			+ TDH_SOCKET_SEQ_ID_LENGTH)) = htonl(REV);                             \
	*((uint32_t *) (POS + TDHS_MAGIC_CODE_SIZE + TDH_SOCKET_COMAND_LENGTH          \
			+ TDH_SOCKET_SEQ_ID_LENGTH + TDH_SOCKET_REVERSE_LENGTH)) = htonl(DATA);\

#define WRITE_PACKET_HEADER_INFO(PACKET,POS)                                      \
		  WRITE_HEADER_INFO(PACKET->command_id_or_response_code,                   \
				PACKET->seq_id,PACKET->reserved,PACKET->length,POS)

int tdhs_encode(easy_request_t *r, void *data) {
	tdhs_packet_t *packet;
	easy_buf_t *b;
	packet = (tdhs_packet_t *) data;
	tb_assert(packet!=NULL);

	while (packet) {
		tb_assert(packet->wbuff!=NULL);
		tb_assert((packet->command_id_or_response_code==CLIENT_STATUS_MULTI_STATUS)?     \
				packet->length==0:packet->length>0);
		b = packet->wbuff;
		//batch请求和返回的buf只包含header
		tb_assert((packet->command_id_or_response_code==CLIENT_STATUS_MULTI_STATUS||     \
				packet->command_id_or_response_code==REQUEST_TYPE_BATCH)?                \
				(uint32_t)(b->last-b->pos)==TDH_SOCKET_HEADER_LENGTH:                    \
				packet->length==((uint32_t)(b->last-b->pos)-TDH_SOCKET_HEADER_LENGTH));

		// 加入header
		WRITE_PACKET_HEADER_INFO(packet, b->pos)
		easy_request_addbuf(r, b);
		packet = packet->next;
	}
	return EASY_OK;
}

} // namespace taobao

