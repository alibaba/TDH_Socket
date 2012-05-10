/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_HTTP_HANDLER_H_
#define EASY_HTTP_HANDLER_H_

#include <easy_define.h>

/**
 * 对ＨＴＴＰ的流解析
 */
EASY_CPP_START

#include <http_parser.h>
#include <easy_io.h>

#define EASY_HTTP_HDR_SIZE              32
#define EASY_HTTP_HDR_HSEED             5
#define EASY_HTTP_CRLF                  "\r\n"

// http status code
#define EASY_HTTP_STATUS_200            "200 OK"
#define EASY_HTTP_STATUS_201            "201 Created"
#define EASY_HTTP_STATUS_202            "202 Accepted"
#define EASY_HTTP_STATUS_204            "204 No Content"
#define EASY_HTTP_STATUS_206            "206 Partial Content"
#define EASY_HTTP_STATUS_301            "301 Moved Permanently"
#define EASY_HTTP_STATUS_302            "302 Moved Temporarily"
#define EASY_HTTP_STATUS_303            "303 See Other"
#define EASY_HTTP_STATUS_304            "304 Not Modified"
#define EASY_HTTP_STATUS_400            "400 Bad Request"
#define EASY_HTTP_STATUS_401            "401 Unauthorized"
#define EASY_HTTP_STATUS_402            "402 Payment Required"
#define EASY_HTTP_STATUS_403            "403 Forbidden"
#define EASY_HTTP_STATUS_404            "404 Not Found"
#define EASY_HTTP_STATUS_405            "405 Not Allowed"
#define EASY_HTTP_STATUS_406            "406 Not Acceptable"
#define EASY_HTTP_STATUS_408            "408 Request Time-out"
#define EASY_HTTP_STATUS_409            "409 Conflict"
#define EASY_HTTP_STATUS_410            "410 Gone"
#define EASY_HTTP_STATUS_411            "411 Length Required"
#define EASY_HTTP_STATUS_412            "412 Precondition Failed"
#define EASY_HTTP_STATUS_413            "413 Request Entity Too Large"
#define EASY_HTTP_STATUS_415            "415 Unsupported Media Type"
#define EASY_HTTP_STATUS_416            "416 Requested Range Not Satisfiable"
#define EASY_HTTP_STATUS_500            "500 Internal Server Error"
#define EASY_HTTP_STATUS_501            "501 Method Not Implemented"
#define EASY_HTTP_STATUS_502            "502 Bad Gateway"
#define EASY_HTTP_STATUS_503            "503 Service Temporarily Unavailable"
#define EASY_HTTP_STATUS_504            "504 Gateway Time-out"
#define EASY_HTTP_STATUS_507            "507 Insufficient Storage"

typedef struct easy_http_request_t      easy_http_request_t;
typedef struct easy_http_header_t       easy_http_header_t;
typedef struct easy_http_header_hash_t  easy_http_header_hash_t;
typedef struct easy_http_packet_t       easy_http_packet_t;

struct easy_http_header_t {
    easy_buf_string_t       name;
    easy_buf_string_t       value;
    easy_http_header_t      *next;
};

struct easy_http_header_hash_t {
    easy_http_header_t      *buckets[EASY_HTTP_HDR_SIZE];
    uint32_t                count;
};

#define easy_http_header_hash_foreach(i, header, table)         \
    for(i = 0; i < EASY_HTTP_HDR_SIZE; i++)                     \
        for(header = (table)->buckets[i]; header; header = header->next)

struct easy_http_request_t {
    easy_message_t          *m;
    http_parser             parser;

    easy_buf_string_t       str_path;
    easy_buf_string_t       str_query_string;
    easy_buf_string_t       str_fragment;
    easy_buf_string_t       str_body;

    easy_http_header_hash_t headers_in;
    easy_http_header_t      *last_header;

    // response
    easy_buf_string_t       status_line;
    easy_http_header_hash_t headers_out;
    easy_list_t             output;
    easy_buf_string_t       content_type;
    int64_t                 content_length;

    // flags
    unsigned int            message_begin_called : 1;
    unsigned int            header_complete_called : 1;
    unsigned int            message_complete_called : 1;
    unsigned int            last_was_value : 1;
    unsigned int            keep_alive : 1;
    unsigned int            is_raw_header : 1;

    int                     parsed_byte;
};

struct easy_http_packet_t {
    easy_buf_string_t       str_query_string;
    easy_buf_string_t       str_path;
    easy_http_header_hash_t headers_out;
    easy_list_t             output;

    unsigned int            is_raw_header : 1;
    unsigned int            post_method : 1;
    unsigned int            keep_alive : 1;
};

void easy_http_add_header(easy_pool_t *pool, easy_http_header_hash_t *table, const char *name, const char *value);
char *easy_http_del_header(easy_http_header_hash_t *table, const char *name);
char *easy_http_get_header(easy_http_header_hash_t *table, const char *name);

void easy_http_header_hash_add(easy_http_header_hash_t *table, easy_http_header_t *header);
easy_http_header_t *easy_http_header_hash_get(easy_http_header_hash_t *table, const char *key, int len);
easy_http_header_t *easy_http_header_hash_del(easy_http_header_hash_t *table, const char *key, int len);

void easy_http_handler_init(easy_io_handler_pt *handler, easy_io_process_pt *process);
void *easy_http_server_on_decode(easy_message_t *m);
int easy_http_server_on_encode(easy_request_t *r, void *data);
void *easy_http_client_on_decode(easy_message_t *m);
int easy_http_client_on_encode(easy_request_t *r, void *data);
easy_http_packet_t *easy_http_packet_create (easy_pool_t *pool);

int easy_url_decode(char *str, int len);
void easy_http_header_string_end(easy_http_request_t *p);
int easy_http_merge_path(char *newpath, int len, const char *rootpath, const char *addpath);
int easy_http_request_printf(easy_http_request_t *r, const char *fmt, ...);

EASY_CPP_END

#endif
