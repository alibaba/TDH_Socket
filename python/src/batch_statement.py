#!/usr/bin/python

#Copyright(C) 2011-2012 Alibaba Group Holding Limited
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Authors:
#   wentong <wentong@taobao.com>

# wentong@taobao.com
# 12-2-20
#
from _struct import unpack
from exception import response_exception
from request import *
from response import response
from statement import encode_packet

class batch_statement:
    wbuff = ""
    id = -1
    ids = []

    def __init__(self, client):
        self.client = client

    def append_request(self, type, req):
        _id = self.client.getid()
        self.wbuff += encode_packet(type, _id, req.to_binary())
        self.ids.append(_id)

    def do_single_reponse(self,idx):
        databuff = ""
        data_len = 0
        while True:
            rbuff = self.client.recv(FSIZE)
            magic_code, resp_code, id, reverse, length = unpack(FMT, rbuff)
            self.client.logger.debug("return resp_code[%d] length[%d],id[%d]" % (resp_code, length, id))
            if id != self.ids[idx]:
                raise response_exception("id is error!")
            rbuff = self.client.recv(length)
            if 400 <= resp_code < 600:
                rpos = 0
                error_code, = unpack('!L', rbuff[rpos:rpos + 4])
                if resp_code == CLIENT_STATUS_DB_ERROR:
                    raise response_exception("mysql return code [%d]" % error_code)
                if error_code < len(ERROR_MSG):
                    raise response_exception(ERROR_MSG[error_code])
                else:
                    raise response_exception("unkown error code [%d]" % error_code)
            elif resp_code == CLIENT_STATUS_ACCEPT:
                databuff += rbuff
                data_len += length
            elif resp_code == CLIENT_STATUS_OK:
                databuff += rbuff
                data_len += length
                self.client.logger.debug("return data_length[%d]" % data_len)
                return response(databuff, data_len).parse()
            else:
                raise response_exception("unknown response code [%d]" % resp_code)

    def do_response(self):
        rbuff = self.client.recv(FSIZE)
        magic_code, resp_code, id, reverse, length = unpack(FMT, rbuff)
        self.client.logger.debug("return resp_code[%d] length[%d],id[%d]" % (resp_code, length, id))
        if id != self.id:
            raise response_exception("id is error!")
        if length > 0:
            rbuff = self.client.recv(length)
        if 400 <= resp_code < 600:
            rpos = 0
            error_code, = unpack('!L', rbuff[rpos:rpos + 4])
            if resp_code == CLIENT_STATUS_DB_ERROR:
                raise response_exception("mysql return code [%d]" % error_code)
            if error_code < len(ERROR_MSG):
                raise response_exception(ERROR_MSG[error_code])
            else:
                raise response_exception("unkown error code [%d]" % error_code)
        elif resp_code == CLIENT_STATUS_MULTI_STATUS:
            ret = []
            for i in xrange(len(self.ids)):
                ret.append(self.do_single_reponse(i))
            return ret
        else:
            raise response_exception("unknown response code [%d]" % resp_code)


    def commit(self):
        self.id = self.client.getid()
        batch_buff = encode_packet(REQUEST_TYPE_BATCH, self.id, self.wbuff, len(self.ids))
        self.client.send(batch_buff)
        return self.do_response()


    def add_delete(self, db, table, index, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None):
        info = table_info(db, table, index, [])
        info.need_field = False
        req = get(info, key, find_flag, start, limit, filters)
        self.append_request.append(REQUEST_TYPE_DELETE, req)


    def add_update(self, db, table, index, fields, updates, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None ):
        get_req = get(table_info(db, table, index, fields), key, find_flag, start, limit, filters)
        req = update(get_req, updates)
        self.append_request(REQUEST_TYPE_UPDATE, req)

    def add_insert(self, db, table, fields, values):
        info = table_info(db, table, None, fields)
        req = insert(info, values)
        self.append_request(REQUEST_TYPE_INSERT, req)


