/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_decode_request_binary.cpp
 *
 *  Created on: 2011-11-21
 *      Author: wentong
 */
#include "tdh_socket_decode_request_binary.hpp"
#include  <arpa/inet.h>

namespace taobao {

static TDHS_INLINE int decode_to_get(tdhs_request_t & req,
		tdhs_packet_t& packet);

static TDHS_INLINE int decode_to_update(tdhs_request_t & req,
		tdhs_packet_t& packet);

static TDHS_INLINE int decode_to_delete_or_count(tdhs_request_t & req,
		tdhs_packet_t& packet);

static TDHS_INLINE int decode_to_insert(tdhs_request_t & req,
		tdhs_packet_t& packet);

static TDHS_INLINE int decode_to_batch(tdhs_request_t & req,
		tdhs_packet_t& packet);

int decode_request_by_binary(tdhs_request_t &req, tdhs_packet_t& packet) {
	easy_info_log("TDHS:use binary to decode request");
	req.type = (tdhs_request_type_t) packet.command_id_or_response_code;
	switch (req.type) {
	case REQUEST_TYPE_GET:
		easy_debug_log("TDHS:decode_to_get");
		return decode_to_get(req, packet);
		break;
	case REQUEST_TYPE_COUNT:
			easy_debug_log("TDHS:decode_to_count");
		return decode_to_delete_or_count(req, packet);
			break;
	case REQUEST_TYPE_UPDATE:
		easy_debug_log("TDHS:decode_to_update");
		return decode_to_update(req, packet);
		break;
	case REQUEST_TYPE_DELETE:
		easy_debug_log("TDHS:decode_to_delete");
		return decode_to_delete_or_count(req, packet);
		break;
	case REQUEST_TYPE_INSERT:
		easy_debug_log("TDHS:decode_to_insert");
		return decode_to_insert(req, packet);
		break;
	case REQUEST_TYPE_BATCH:
		easy_debug_log("TDHS:decode_to_batch");
		return decode_to_batch(req, packet);
		break;
	default:
		easy_warn_log("TDHS:can't find right request type! type is [%d]",
				req.type);
		return EASY_ERROR;
		break;
	}
}

static TDHS_INLINE int _decode_to_table_info(tdhs_request_table_t& table_info,
		uint32_t &read_len, char* &pos, bool need_field) {
	//read db
	read_uint32_ref(table_info.db.len, pos, read_len);
	read_str_ref(table_info.db.str, pos, read_len, table_info.db.len);
	//read table
	read_uint32_ref(table_info.table.len, pos, read_len);
	read_str_ref(table_info.table.str, pos, read_len, table_info.table.len);
	//read index
	read_uint32_ref(table_info.index.len, pos, read_len);
	if (table_info.index.len > 0) {
		read_str_ref(table_info.index.str, pos, read_len, table_info.index.len);
	} else {
		table_info.index.str = PRIMARY;
		table_info.index.len = PRIMARY_SIZE;
	}
	//read field
	read_uint32_ref(table_info.field_num, pos, read_len);
	for (uint32_t i = 0; i < table_info.field_num; i++) {
		if (i < REQUEST_MAX_FIELD_NUM) {
			tdhs_string_t& field = table_info.fields[i];
			read_uint32_ref(field.len, pos, read_len);
			read_str_ref(field.str, pos, read_len, field.len);
		} else {
			//skip too many field
			tdhs_string_t temp_field;
			read_uint32_ref(temp_field.len, pos, read_len);
			read_str_ref(temp_field.str, pos, read_len, temp_field.len);
		}
	}
	if (table_info.is_vaild(need_field) != EASY_OK) { //table 信息读完..先判断是否有效
		return EASY_ERROR;
	}
	return EASY_OK;
}

static TDHS_INLINE int _decode_to_filter(tdhs_filter_t& filter,
		uint32_t &read_len, char* &pos) {
	uint8_t tmp8 = 0;
	read_uint32_ref(filter.filter_num, pos, read_len);
	for (uint32_t i = 0; i < filter.filter_num; i++) {
		if (i < REQUEST_MAX_FIELD_NUM) {
			tdhs_string_t& field = filter.field[i];
			read_uint32_ref(field.len, pos, read_len);
			read_str_ref(field.str, pos, read_len, field.len);
			read_uint8_ref(tmp8, pos, read_len);
			if (filter.set_flag(i, tmp8) != EASY_OK) {
				return EASY_ERROR;
			}
			tdhs_string_t& value = filter.value[i];
			read_uint32_ref(value.len, pos, read_len);
			read_str_ref(value.str, pos, read_len, value.len);
		} else {
			//skip too many data
			tdhs_string_t temp_field;
			read_uint32_ref(temp_field.len, pos, read_len);
			read_str_ref(temp_field.str, pos, read_len, temp_field.len);
			read_uint8_ref(tmp8, pos, read_len);
			tdhs_string_t temp_value;
			read_uint32_ref(temp_value.len, pos, read_len);
			read_str_ref(temp_value.str, pos, read_len, temp_value.len);
		}
	}
	filter.print_log();
	return EASY_OK;
}

static TDHS_INLINE int _decode_to_get(tdhs_request_t & req, uint32_t &read_len,
		char* &pos) {
	if (_decode_to_table_info(req.table_info, read_len, pos, true) != EASY_OK) {
		return EASY_ERROR;
	}
	//read key
	read_uint32_ref(req.get.key_num, pos, read_len);
	req.get.keys = req.get.r_keys;
	for (uint32_t i = 0; i < req.get.key_num; i++) {
		if (i < REQUEST_MAX_KEY_NUM) {
			tdhs_get_key_t& one_key = req.get.keys[i];
			read_uint32_ref(one_key.key_field_num, pos, read_len);
			for (uint32_t j = 0; j < one_key.key_field_num; j++) {
				tdhs_string_t& key = one_key.key[j];
				read_uint32_ref(key.len, pos, read_len);
				read_str_ref(key.str, pos, read_len, key.len);
			}
		} else {
			//skip too many field
			tdhs_get_key_t temp_key;
			read_uint32_ref(temp_key.key_field_num, pos, read_len);
			for (uint32_t j = 0; j < temp_key.key_field_num; j++) {
				tdhs_string_t& key = temp_key.key[j];
				read_uint32_ref(key.len, pos, read_len);
				read_str_ref(key.str, pos, read_len, key.len);
			}

		}
	}
	//read flag
	uint8_t tmp8 = 0; //for convert
	read_uint8_ref(tmp8, pos, read_len);
	req.get.find_flag = (tdhs_find_flag_t) tmp8;
	//read start and limit
	read_uint32_ref(req.get.start, pos, read_len);
	read_uint32_ref(req.get.limit, pos, read_len);

    if(req.get.key_num<2 && req.get.find_flag == TDHS_BETWEEN){
        easy_warn_log("TDHS:too few keys where flag is between condition!");
        return ERROR_OUT_OF_IN;
    }

	if (req.get.is_vaild() != EASY_OK) { //get 信息读完..先判断是否有效
		return EASY_ERROR;
	}

	if (_decode_to_filter(req.get.filter, read_len, pos) != EASY_OK) {
		return EASY_ERROR;
	}

	return EASY_OK;
}

static TDHS_INLINE int decode_to_get(tdhs_request_t & req,
		tdhs_packet_t& packet) {
	uint32_t read_len = packet.length;
	char* pos = packet.rdata;
	if (_decode_to_get(req, read_len, pos) != EASY_OK) {
		return EASY_ERROR;
	}
	if (read_len != 0) {
		easy_warn_log("TDHS:too long data!");
		return EASY_ERROR;
	}
	return EASY_OK;
}

static TDHS_INLINE int decode_to_update(tdhs_request_t & req,
		tdhs_packet_t& packet) {
	uint8_t tmp8 = 0;
	uint32_t read_len = packet.length;
	char* pos = packet.rdata;
	if (_decode_to_get(req, read_len, pos) != EASY_OK) {
		return EASY_ERROR;
	}
	//read value
	read_uint32_ref(req.values.value_num, pos, read_len);
	if (req.values.value_num != req.table_info.field_num) {
		easy_warn_log("TDHS:update field_num[%d] != value_num[%d]",
				req.table_info.field_num, req.values.value_num);
		return EASY_ERROR;
	}
	for (uint32_t i = 0; i < req.values.value_num; i++) {
		if (i < REQUEST_MAX_FIELD_NUM) {
			read_uint8_ref(tmp8, pos, read_len);
			if (req.values.set_flag(i, tmp8) != EASY_OK) {
				return EASY_ERROR;
			}
			tdhs_string_t& value = req.values.value[i];
			read_uint32_ref(value.len, pos, read_len);
			read_str_ref(value.str, pos, read_len, value.len);
		} else {
			//skip too many field
			read_uint8_ref(tmp8, pos, read_len);
			tdhs_string_t temp_value;
			read_uint32_ref(temp_value.len, pos, read_len);
			read_str_ref(temp_value.str, pos, read_len, temp_value.len);
		}
	}
	req.values.print_log();
	if (read_len != 0) {
		easy_warn_log("TDHS:too long data!");
		return EASY_ERROR;
	}
	return EASY_OK;
}

static TDHS_INLINE int decode_to_delete_or_count(tdhs_request_t & req,
		tdhs_packet_t& packet) {
	uint32_t read_len = packet.length;
	char* pos = packet.rdata;
	if (_decode_to_table_info(req.table_info, read_len, pos, false) != EASY_OK) {
		return EASY_ERROR;
	}
	//read key
	read_uint32_ref(req.get.key_num, pos, read_len);
	req.get.keys = req.get.r_keys;
	for (uint32_t i = 0; i < req.get.key_num; i++) {
		if (i < REQUEST_MAX_KEY_NUM) {
			tdhs_get_key_t& one_key = req.get.keys[i];
			read_uint32_ref(one_key.key_field_num, pos, read_len);
			for (uint32_t j = 0; j < one_key.key_field_num; j++) {
				tdhs_string_t& key = one_key.key[j];
				read_uint32_ref(key.len, pos, read_len);
				read_str_ref(key.str, pos, read_len, key.len);
			}
		} else {
			//skip too many field
			tdhs_get_key_t temp_key;
			read_uint32_ref(temp_key.key_field_num, pos, read_len);
			for (uint32_t j = 0; j < temp_key.key_field_num; j++) {
				tdhs_string_t& key = temp_key.key[j];
				read_uint32_ref(key.len, pos, read_len);
				read_str_ref(key.str, pos, read_len, key.len);
			}

		}
	}
	//read flag
	uint8_t tmp8 = 0; //for convert
	read_uint8_ref(tmp8, pos, read_len);
	req.get.find_flag = (tdhs_find_flag_t) tmp8;
	//read start and limit
	read_uint32_ref(req.get.start, pos, read_len);
	read_uint32_ref(req.get.limit, pos, read_len);

    if(req.get.key_num<2 && req.get.find_flag == TDHS_BETWEEN){
        easy_warn_log("TDHS:too few keys where flag is between condition!");
        return ERROR_OUT_OF_IN;
    }

	if (req.get.is_vaild() != EASY_OK) { //get 信息读完..先判断是否有效
		return EASY_ERROR;
	}

	if (_decode_to_filter(req.get.filter, read_len, pos) != EASY_OK) {
		return EASY_ERROR;
	}
	if (read_len != 0) {
		easy_warn_log("TDHS:too long data!");
		return EASY_ERROR;
	}
	return EASY_OK;
}

static TDHS_INLINE int decode_to_insert(tdhs_request_t & req,
		tdhs_packet_t& packet) {
	uint8_t tmp8 = 0;
	uint32_t read_len = packet.length;
	char* pos = packet.rdata;
	if (_decode_to_table_info(req.table_info, read_len, pos, false) != EASY_OK) {
		return EASY_ERROR;
	}

	//read value
	read_uint32_ref(req.values.value_num, pos, read_len);
	if (req.values.value_num != req.table_info.field_num) {
		easy_warn_log("TDHS:update field_num[%d] != value_num[%d]",
				req.table_info.field_num, req.values.value_num);
		return EASY_ERROR;
	}
	for (uint32_t i = 0; i < req.values.value_num; i++) {
		if (i < REQUEST_MAX_FIELD_NUM) {
			read_uint8_ref(tmp8, pos, read_len);
			if (req.values.set_flag(i, tmp8) != EASY_OK) {
				return EASY_ERROR;
			}
			tdhs_string_t& value = req.values.value[i];
			read_uint32_ref(value.len, pos, read_len);
			read_str_ref(value.str, pos, read_len, value.len);
		} else {
			//skip too many field
			read_uint8_ref(tmp8, pos, read_len);
			tdhs_string_t temp_value;
			read_uint32_ref(temp_value.len, pos, read_len);
			read_str_ref(temp_value.str, pos, read_len, temp_value.len);
		}
	}
	req.values.print_log();
	if (read_len != 0) {
		easy_warn_log("TDHS:too long data!");
		return EASY_ERROR;
	}
	return EASY_OK;
}

static TDHS_INLINE int decode_to_batch(tdhs_request_t & req,
		tdhs_packet_t& packet) {
	tdhs_packet_t *batch_packet = packet.next;
	while (batch_packet) {
		if (decode_request_by_binary(batch_packet->req,
				*batch_packet) != EASY_OK) {
			return EASY_ERROR;
		}
		if (batch_packet->req.type == REQUEST_TYPE_GET
				|| batch_packet->req.type == REQUEST_TYPE_COUNT) {
			easy_error_log("TDHS:batch request can't include read request!");
			return EASY_ERROR;
		}
		batch_packet = batch_packet->next;
	}
	return EASY_OK;
}

} // namespace taobao

