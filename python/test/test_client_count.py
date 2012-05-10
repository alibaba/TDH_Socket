#!/usr/bin/python
#Copyright(C) 2011-2012 Alibaba Group Holding Limited
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# Authors:
#   wentong <wentong@taobao.com>
# 11-9-28
#
from  block_tdhs_client import *
from exception import response_exception

con_num = 1
connect_pool = []

for i in xrange(con_num):
    connect_pool.append(ConnectionManager("t-wentong", read_code="ab", write_code="cd", time_out=10000000))

for i in xrange(1):
    connect = connect_pool[i % con_num]
    try:
        field_types, records = connect.count(u"jdbc_test", u"orders", None, [["8"]],
            TDHS_LE, 0, 0)
        print field_types
        print len(records)
        for r in records:
            print r
    except response_exception, e:
        print e

for c in connect_pool:
    c.close()

