# coding=UTF8
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



TDHS_HEAD = "TDHS"
TDHS_VERSION_BSON = 0 #use bson
TDHS_VERSION_BINARY = 1 #use binary

FMT = '!LLLLL'
FSIZE = 20 # LLLL

REQUEST_TYPE_GET = 0
REQUEST_TYPE_COUNT = 1
REQUEST_TYPE_UPDATE = 10
REQUEST_TYPE_DELETE = 11
REQUEST_TYPE_INSERT = 12
REQUEST_TYPE_BATCH = 20
REQUEST_TYPE_SHAKE_HANDS = 0xFFFF

MAGIC_CODE = 0xFFFFFFFF

#find_flag
TDHS_EQ = 0     # = for asc
TDHS_GE = 1     # >=
TDHS_LE = 2     # <=
TDHS_GT = 3     # >
TDHS_LT = 4     # <
TDHS_IN = 5     # in
TDHS_DEQ = 6    # = for desc
TDHS_BETWEEN = 7    # between

#filter flag
TDHS_FILTER_EQ = 0     # =
TDHS_FILTER_GE = 1     # >=
TDHS_FILTER_LE = 2     # <=
TDHS_FILTER_GT = 3     # >
TDHS_FILTER_LT = 4     # <
TDHS_FILTER_NOT = 5    # !


#update flag
TDHS_UPDATE_SET = 0 # =
TDHS_UPDATE_ADD = 1 # +
TDHS_UPDATE_SUB = 2 # -
TDHS_UPDATE_NOW = 3 # now()
TDHS_UPDATE_END = 4

#return to client status
#正确返回status
CLIENT_STATUS_OK = 200 #完成所有数据的返回
CLIENT_STATUS_ACCEPT = 202 #对于流的处理,还有未返回的数据
CLIENT_STATUS_MULTI_STATUS = 207 #对于batch请求的返回,表示后面跟了多个请求
#请求导致的错误信息
CLIENT_STATUS_BAD_REQUEST = 400
CLIENT_STATUS_FORBIDDEN = 403 #没权限
CLIENT_STATUS_NOT_FOUND = 404 #没有找到资源,如 db/table/index 等
CLIENT_STATUS_REQUEST_TIME_OUT = 408 #超时
#服务器导致的错误信息
CLIENT_STATUS_SERVER_ERROR = 500 #server无法处理的错误,比如内存不够
CLIENT_STATUS_NOT_IMPLEMENTED = 501 #server没实现这个功能
CLIENT_STATUS_DB_ERROR = 502 #handler返回的错误码
CLIENT_STATUS_SERVICE_UNAVAILABLE = 503 #负载过重

#error code
CLIENT_ERROR_CODE_FAILED_TO_OPEN_TABLE = 1
CLIENT_ERROR_CODE_FAILED_TO_OPEN_INDEX = 2
CLIENT_ERROR_CODE_FAILED_TO_MISSING_FIELD = 3
CLIENT_ERROR_CODE_FAILED_TO_MATCH_KEY_NUM = 4
CLIENT_ERROR_CODE_FAILED_TO_LOCK_TABLE = 5
CLIENT_ERROR_CODE_NOT_ENOUGH_MEMORY = 6
CLIENT_ERROR_CODE_DECODE_REQUEST_FAILED = 7
CLIENT_ERROR_CODE_FAILED_TO_MISSING_FIELD_IN_FILTER_OR_USE_BLOB = 8
CLIENT_ERROR_CODE_FAILED_TO_COMMIT = 9
CLIENT_ERROR_CODE_NOT_IMPLEMENTED = 10
CLIENT_ERROR_CODE_REQUEST_TIME_OUT = 11
CLIENT_ERROR_CODE_UNAUTHENTICATION = 12
CLIENT_ERROR_CODE_KILLED = 13
CLIENT_ERROR_CODE_THROTTLED = 14


#error code message
ERROR_MSG = [
    "nothing",
    "TDH_SOCKET failed to open table!",
    "TDH_SOCKET failed to open index!",
    "TDH_SOCKET field is missing!",
    "TDH_SOCKET request can't match the key number!",
    "TDH_SOCKET failed to lock table!",
    "TDH_SOCKET server don't have enough memory!",
    "TDH_SOCKET server can't decode your request!",
    "TDH_SOCKET field is missing in filter or use blob!",
    "TDH_SOCKET failed to commit, will be rollback!",
    "TDH_SOCKET not implemented!",
    "TDH_SOCKET request time out!",
    "TDH_SOCKET request is unauthentication!",
    "TDH_SOCKET request is killed!",
    "TDH_SOCKET request is throttled!"
]

#field type
MYSQL_TYPE_DECIMAL = 0
MYSQL_TYPE_TINY = 1
MYSQL_TYPE_SHORT = 2
MYSQL_TYPE_LONG = 3
MYSQL_TYPE_FLOAT = 4
MYSQL_TYPE_DOUBLE = 5
MYSQL_TYPE_NULL = 6
MYSQL_TYPE_TIMESTAMP = 7
MYSQL_TYPE_LONGLONG = 8
MYSQL_TYPE_INT24 = 9
MYSQL_TYPE_DATE = 10
MYSQL_TYPE_TIME = 11
MYSQL_TYPE_DATETIME = 12
MYSQL_TYPE_YEAR = 13
MYSQL_TYPE_NEWDATE = 14
MYSQL_TYPE_VARCHAR = 15
MYSQL_TYPE_BIT = 16
MYSQL_TYPE_NEWDECIMAL = 246
MYSQL_TYPE_ENUM = 247
MYSQL_TYPE_SET = 248
MYSQL_TYPE_TINY_BLOB = 249
MYSQL_TYPE_MEDIUM_BLOB = 250
MYSQL_TYPE_LONG_BLOB = 251
MYSQL_TYPE_BLOB = 252
MYSQL_TYPE_VAR_STRING = 253
MYSQL_TYPE_STRING = 254
MYSQL_TYPE_GEOMETRY = 255




