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
# 11-9-28
#
from _struct import pack
import socket
from batch_statement import batch_statement
import logger
from statement import statement
from tdhs_common import *

import threading

def add_str_to_binary(s):
    if s:
        es = s.encode("GBK")
        return pack('!L', len(es) + 1) + pack(str(len(es)) + 's', es) + '\x00' #for c string
    else:
        return pack('!L', 0)


class TDHSConnection:
    id = 0

    id_lock = threading.Lock()

    logger = logger.logger()

    def __init__(self, host, port=9999, time_out=1000, read_code=None, write_code=None):
        self.addr = (host, port)
        self.time_out = time_out
        self.sock = socket.socket(socket.AF_INET)
        self.sock.connect((host, port))
        self.__shake_hands(read_code, write_code)

    def __shake_hands(self, read_code, write_code):
        shake_hands = TDHS_HEAD
        wbuf = shake_hands
        wbuf += pack('!L', TDHS_VERSION_BINARY)
        wbuf += pack('!L', self.time_out)
        wbuf += add_str_to_binary(read_code)
        wbuf += add_str_to_binary(write_code)
        self.send(pack(FMT, MAGIC_CODE, REQUEST_TYPE_SHAKE_HANDS, self.getid(), 0, len(wbuf)) + wbuf)

    def getid(self):
        try:
            self.id_lock.acquire()
            self.id += 1
            return self.id
        finally:
            self.id_lock.release()

    def send(self, wbuf):
        self.logger.debug("send [%s]" % wbuf)
        self.sock.send(wbuf)

    def recv(self, size):
        length = size
        rbuff = ""
        while length > 0:
            b = self.sock.recv(length)
            rbuff += b
            length -= len(b)
        return  rbuff


    def get(self, db, table, index, fields, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None):
        sm = self.get_statement()
        return sm.get(db, table, index, fields, key, find_flag, start, limit, filters)

    def delete(self, db, table, index, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None):
        sm = self.get_statement()
        return sm.delete(db, table, index, key, find_flag, start, limit, filters)

    def count(self, db, table, index, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None):
        sm = self.get_statement()
        return sm.count(db, table, index, key, find_flag, start, limit, filters)


    def update(self, db, table, index, fields, updates, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None ):
        sm = self.get_statement()
        return sm.update(db, table, index, fields, updates, key, find_flag, start, limit, filters)

    def insert(self, db, table, fields, values):
        sm = self.get_statement()
        return sm.insert(db, table, fields, values)


    def get_statement(self, hash=0):
        return statement(self, hash)

    def get_batch_statement(self):
        return batch_statement(self)


    def close(self):
        self.sock.close()


def ConnectionManager(host, port=9999, time_out=1000, read_code=None, write_code=None):
    return  TDHSConnection(host, port, time_out, read_code, write_code)
