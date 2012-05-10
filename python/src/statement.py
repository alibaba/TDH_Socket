#!/usr/bin/python
#Copyright(C) 2011-2012 Alibaba Group Holding Limited
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Authors:
#   wentong <wentong@taobao.com>
# 12-2-20
#
from exception import *
from request import *
from response import *
from tdhs_common import *

def encode_packet(type, id, wbuff, reserved=0):
    buf = pack(FMT, MAGIC_CODE, type, id, reserved, len(wbuff))
    buf += wbuff
    return buf


class statement:
    id = -1

    hash = 0

    def __init__(self, client, h=0):
        self.client = client
        self.hash = h

    def getid(self):
        self.id = self.client.getid()
        return self.id

    def do_reponse(self):
        databuff = ""
        data_len = 0
        while True:
            rbuff = self.client.recv(FSIZE)
            magic_code, resp_code, id, reverse, length = unpack(FMT, rbuff)
            self.client.logger.debug("return resp_code[%d] length[%d],id[%d]" % (resp_code, length, id))
            if id != self.id:
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


    def get(self, db, table, index, fields, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None):
        req = get(table_info(db, table, index, fields), key, find_flag, start, limit, filters)
        self.client.send(encode_packet(REQUEST_TYPE_GET, self.getid(), req.to_binary()))
        return self.do_reponse()

    def delete(self, db, table, index, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None):
        info = table_info(db, table, index, [])
        info.need_field = False
        req = get(info, key, find_flag, start, limit, filters)
        self.client.send(encode_packet(REQUEST_TYPE_DELETE, self.getid(), req.to_binary(), self.hash))
        return self.do_reponse()


    def count(self, db, table, index, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None):
        info = table_info(db, table, index, [])
        info.need_field = False
        req = get(info, key, find_flag, start, limit, filters)
        self.client.send(encode_packet(REQUEST_TYPE_COUNT, self.getid(), req.to_binary(), self.hash))
        return self.do_reponse()

    def update(self, db, table, index, fields, updates, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None ):
        get_req = get(table_info(db, table, index, fields), key, find_flag, start, limit, filters)
        req = update(get_req, updates)
        self.client.send(encode_packet(REQUEST_TYPE_UPDATE, self.getid(), req.to_binary(), self.hash))
        return self.do_reponse()

    def insert(self, db, table, fields, values):
        info = table_info(db, table, None, fields)
        req = insert(info, values)
        self.client.send(encode_packet(REQUEST_TYPE_INSERT, self.getid(), req.to_binary(), self.hash))
        return self.do_reponse()