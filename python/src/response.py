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
from _struct import unpack

class response:
    def __init__(self, rbuff, len):
        self.rbuff = rbuff
        self.len = len

    def parse(self):
        rpos = 0
        records = []
        field_num, = unpack('!L', self.rbuff[rpos:rpos + 4])
        rpos += 4
        field_types = []
        for i in xrange(field_num):
            type, = unpack('!B', self.rbuff[rpos:rpos + 1])
            field_types.append(type)
            rpos += 1
        while rpos < self.len:
            record = []
            for i in xrange(field_num):
                data_len, = unpack('!L', self.rbuff[rpos:rpos + 4])
                rpos += 4
                if data_len > 0:
                    c_fmt = str(data_len) + 's'
                    field_value, = unpack(c_fmt, self.rbuff[rpos:rpos + data_len])
                    rpos += data_len
                    if data_len == 1 and field_value == '\0':
                        record.append("")
                    else:
                        record.append(field_value)
                else:
                    record.append(None)
            records.append(record)
        return field_types, records