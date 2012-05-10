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

con_num = 1
connect_pool = []

for i in xrange(con_num):
    connect_pool.append(ConnectionManager("t-wentong"))

for i in xrange(1):
    connect = connect_pool[i % con_num]
    field_types, records = connect.get(u"test", u"s", None,[u"id",u"data"], [[u"3"],[u"4",u"1"]],
        TDHS_IN, 0, 0)
    print field_types
    print len(records)
    for r in records:
        print r

for c in connect_pool:
    c.close()

