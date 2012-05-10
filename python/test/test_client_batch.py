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
    connect_pool.append(ConnectionManager("t-wentong",time_out=10000000))

for i in xrange(1):
    connect = connect_pool[i % con_num]
    batch = connect.get_batch_statement()
    batch.add_insert(u"test", u"test", [u"data"], [[TDHS_UPDATE_SET,u"1"]])
    batch.add_insert(u"test", u"test", [u"data"], [ [TDHS_UPDATE_SET,u"2"]])
#    batch.add_insert(u"test", u"t", [u"a", u"b"], [[TDHS_UPDATE_SET,u"2222"], [TDHS_UPDATE_SET,u"3"]])
    ret = batch.commit()

    for r in ret:
        print r

for c in connect_pool:
    c.close()

