#!/usr/bin/python
#Copyright(C) 2011-2012 Alibaba Group Holding Limited
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Authors:
#   wentong <wentong@taobao.com>
# 11-11-22
#
from exception import request_exception
from tdhs_common import *
from _struct import pack

def add_str_to_binary(s):
    if s:
        es = s.encode("GBK")
        return pack('!L', len(es) + 1) + pack(str(len(es)) + 's', es) + '\x00' #for c string
    else:
        return  pack('!L', 0)


def add_array_to_binary(array):
    v = pack('!L', len(array))
    for a in array:
        if isinstance(a, list) or isinstance(a, tuple):
            v += add_array_to_binary(a)
        else:
            v += add_str_to_binary(a)
    return v


class table_info:
    def __init__(self, db, table, index, fields):
        self.db = db.lower()
        self.table = table.lower()
        self.index = index
        self.fields = fields
        self.need_field = True

    def is_vaild(self):
        if self.db and self.table:
            if self.need_field and not self.fields:
                return False
            return True
        else:
            return False

    def to_binary(self):
        if not self.is_vaild():
            raise request_exception("table_info is not valid!")
        req = add_str_to_binary(self.db)
        req += add_str_to_binary(self.table)
        if self.index:
            req += add_str_to_binary(self.index)
        else:
            req += pack('!L', 0)
        req += add_array_to_binary(self.fields)
        return req


class filter:
    def __init__(self):
        self.num = 0
        self.field = []
        self.flag = []
        self.value = []

    def add_filter(self, field, flag, value):
        if flag > TDHS_FILTER_NOT or flag < TDHS_FILTER_EQ:
            raise request_exception("unkonw filter flag")
        self.field.append(field)
        self.flag.append(flag)
        self.value.append(value)
        self.num += 1

    def to_binary(self):
        req = pack('!L', self.num)
        for i in xrange(self.num):
            req += add_str_to_binary(self.field[i])
            req += pack('!B', self.flag[i])
            req += add_str_to_binary(self.value[i])
        return req


class get:
    def __init__(self, table_info, key, find_flag=TDHS_EQ, start=0, limit=0, filters=None):
        self.table_info = table_info
        self.key = key
        self.find_flag = find_flag
        self.start = start
        self.limit = limit
        self.filter = filter()
        if filters:
            for f in filters:
                self.add_filter(f[0], f[1], f[2])

    def add_filter(self, field, flag, value):
        if not self.filter:
            self.filter = filter()
        self.filter.add_filter(field, flag, value)

    def is_vaild(self):
        if self.table_info.is_vaild() and self.key and self.key is not None and len(self.key) > 0\
           and self.limit is not None and self.find_flag is not None and self.find_flag >= TDHS_EQ\
        and self.find_flag <= TDHS_BETWEEN:
            return True
        else:
            return False

    def to_binary(self):
        if not self.is_vaild():
            raise request_exception("get is not valid!")
        req = self.table_info.to_binary()
        req += add_array_to_binary(self.key)
        req += pack('!B', self.find_flag)
        req += pack('!L', self.start)
        req += pack('!L', self.limit)
        req += self.filter.to_binary()
        return req


class update:
    def __init__(self, get, updates):
        self.num = 0
        self.flag = []
        self.value = []
        self.get = get
        if updates:
            for u in updates:
                self.add_update(u[0], u[1])


    def add_update(self, flag, value):
        if flag >=TDHS_UPDATE_END or flag < TDHS_UPDATE_SET:
            raise request_exception("unkonw update flag")
        self.flag.append(flag)
        self.value.append(value)
        self.num += 1

    def is_vaild(self):
        if self.get.is_vaild() and  self.num == len(self.get.table_info.fields):
            return True
        else:
            return False


    def to_binary(self):
        if not self.is_vaild():
            raise request_exception("update is not valid!")
        req = self.get.to_binary()
        req += pack('!L', self.num)
        for i in xrange(self.num):
            req += pack('!B', self.flag[i])
            req += add_str_to_binary(self.value[i])
        return req


class insert:
    def __init__(self, table_info, values):
        self.table_info = table_info
        self.flag = []
        self.value = []
        self.num = 0
        if values:
            for v in values:
                self.add_value(v[0],v[1])


    def add_value(self, flag, value):
        if flag >=TDHS_UPDATE_END or flag < TDHS_UPDATE_SET:
            raise request_exception("unkonw update flag")
        self.flag.append(flag)
        self.value.append(value)
        self.num += 1

    def is_vaild(self):
        if self.table_info.is_vaild() and  self.num == len(self.table_info.fields):
            return True
        else:
            return False

    def to_binary(self):
        if not self.is_vaild():
            raise request_exception("insert is not valid!")
        req = self.table_info.to_binary()
        req += pack('!L', self.num)
        for i in xrange(self.num):
            req += pack('!B', self.flag[i])
            req += add_str_to_binary(self.value[i])
        return req


class batch:
    pass