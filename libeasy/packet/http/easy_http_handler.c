/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#include <ctype.h>
#include "easy_http_handler.h"

static int easy_http_request_message_begin(http_parser *parser);
static int easy_http_request_headers_complete(http_parser *parser);
static int easy_http_request_message_complete(http_parser *parser);
static int easy_http_request_on_path(http_parser *, const char *, size_t);
static int easy_http_request_on_query_string(http_parser *, const char *, size_t);
static int easy_http_request_on_fragment(http_parser *, const char *, size_t);
static int easy_http_request_on_body(http_parser *, const char *, size_t);
static int easy_http_request_on_header_field (http_parser *, const char *, size_t);
static int easy_http_request_on_header_value (http_parser *, const char *, size_t);
static int easy_http_request_create (easy_message_t *m, enum http_parser_type type);
static void easy_http_string_end(easy_buf_string_t *s);

/**
 * easy http request settings
 */
static http_parser_settings easy_http_request_settings = {
    easy_http_request_message_begin,
    easy_http_request_on_path,
    easy_http_request_on_query_string,
    NULL,
    easy_http_request_on_fragment,
    easy_http_request_on_header_field,
    easy_http_request_on_header_value,
    easy_http_request_headers_complete,
    easy_http_request_on_body,
    easy_http_request_message_complete
};
static http_parser_settings easy_http_response_settings = {
    easy_http_request_message_begin,
    NULL, NULL, NULL, NULL,
    easy_http_request_on_header_field,
    easy_http_request_on_header_value,
    easy_http_request_headers_complete,
    easy_http_request_on_body,
    easy_http_request_message_complete
};

static void easy_http_string_end(easy_buf_string_t *s)
{
    char *ptr;

    if (s->len && (ptr = s->data + s->len)) {
        *ptr = '\0';
    }
}

/**
 * 把header对加入到table中
 */
void easy_http_add_header(easy_pool_t *pool, easy_http_header_hash_t *table,
                          const char *name, const char *value)
{
    easy_http_header_t *header;

    header = (easy_http_header_t *)easy_pool_alloc(pool, sizeof(easy_http_header_t));
    easy_buf_string_set(&header->name, name);
    easy_buf_string_set(&header->value, value);

    easy_http_header_hash_add(table, header);
}

/**
 * 删除header
 */
char *easy_http_del_header(easy_http_header_hash_t *table, const char *name)
{
    easy_http_header_t  *t;

    if ((t = easy_http_header_hash_del(table, name, strlen(name))) != NULL)
        return easy_buf_string_ptr(&t->value);

    return NULL;
}

/**
 * 得到一header的value
 */
char *easy_http_get_header(easy_http_header_hash_t *table, const char *name)
{
    easy_http_header_t  *t;

    if ((t = easy_http_header_hash_get(table, name, strlen(name))) != NULL)
        return easy_buf_string_ptr(&t->value);

    return NULL;
}

/**
 * 把header加入table中
 */
void easy_http_header_hash_add(easy_http_header_hash_t *table, easy_http_header_t *header)
{
    uint64_t                n;

    n = easy_hash_code(easy_buf_string_ptr(&header->name),
                       header->name.len, EASY_HTTP_HDR_HSEED);
    n = n & (EASY_HTTP_HDR_SIZE - 1);

    header->next = table->buckets[n];
    table->buckets[n] = header;
    table->count ++;
}

easy_http_header_t *easy_http_header_hash_get(easy_http_header_hash_t *table, const char *key, int len)
{
    uint64_t                n;
    easy_http_header_t      *t;

    n = easy_hash_code(key, len, EASY_HTTP_HDR_HSEED);
    n = n & (EASY_HTTP_HDR_SIZE - 1);
    t = table->buckets[n];

    while (t) {
        if (t->name.len == len && memcmp(key, easy_buf_string_ptr(&t->name), len) == 0) {
            return t;
        }

        t = t->next;
    }

    return NULL;
}

easy_http_header_t *easy_http_header_hash_del(easy_http_header_hash_t *table, const char *key, int len)
{
    uint64_t                n;
    easy_http_header_t      *t, *prev;

    n = easy_hash_code(key, len, EASY_HTTP_HDR_HSEED);
    n = n & (EASY_HTTP_HDR_SIZE - 1);
    t = table->buckets[n];
    prev = NULL;

    while (t) {
        if (t->name.len == len && memcmp(key, easy_buf_string_ptr(&t->name), len) == 0) {
            if (prev)
                prev->next = t->next;
            else
                table->buckets[n] = t->next;

            t->next = NULL;
            table->count --;

            return t;
        }

        prev = t;
        t = t->next;
    }

    return NULL;
}

// http parser callback
static int easy_http_request_message_begin(http_parser *parser)
{
    easy_http_request_t *p = (easy_http_request_t *) parser->data;
    p->message_begin_called = 1;
    return 0;
}
static int easy_http_request_headers_complete(http_parser *parser)
{
    easy_http_request_t *p = (easy_http_request_t *) parser->data;
    p->header_complete_called = 1;
    return 0;
}
static int easy_http_request_message_complete(http_parser *parser)
{
    easy_http_request_t *p = (easy_http_request_t *) parser->data;
    p->message_complete_called = 1;
    return 1;
}

#define EASY_HTTP_REQUEST_CB_DEFINE(name)                                       \
    static int easy_http_request_on_##name(http_parser *parser,                 \
                                           const char *value, size_t len)  {    \
        easy_http_request_t     *p;                                             \
        p = (easy_http_request_t*) parser->data;                                \
        easy_buf_string_append(&p->str_##name, value, len);                     \
        return 0;                                                               \
    }

EASY_HTTP_REQUEST_CB_DEFINE(path);
EASY_HTTP_REQUEST_CB_DEFINE(query_string);
EASY_HTTP_REQUEST_CB_DEFINE(fragment);
EASY_HTTP_REQUEST_CB_DEFINE(body);

static int easy_http_request_on_header_field (http_parser *parser, const char *value, size_t len)
{
    easy_http_request_t         *p;

    p = (easy_http_request_t *) parser->data;

    // new name
    if (p->last_was_value) {
        p->last_header = (easy_http_header_t *) easy_pool_calloc(p->m->pool, sizeof(easy_http_header_t));
    }

    easy_buf_string_append(&p->last_header->name, value, len);
    p->last_was_value = 0;
    return 0;
}

static int easy_http_request_on_header_value (http_parser *parser, const char *value, size_t len)
{
    easy_http_request_t         *p;

    p = (easy_http_request_t *) parser->data;

    // new value
    if (!p->last_was_value) {
        easy_http_header_hash_add(&p->headers_in, p->last_header);
    }

    easy_buf_string_append(&p->last_header->value, value, len);
    p->last_was_value = 1;
    return 0;
}

/**
 * 新创一个easy_http_request_t
 */
static int easy_http_request_create (easy_message_t *m, enum http_parser_type type)
{
    easy_http_request_t     *p;

    if ((p = (easy_http_request_t *) easy_pool_calloc(m->pool, sizeof(easy_http_request_t))) == NULL)
        return EASY_ERROR;

    http_parser_init(&p->parser, type);
    p->parser.data = p;
    p->last_was_value = 1;
    p->m = m;
    p->content_length = -1;
    m->user_data = p;
    easy_list_init(&p->output);

    return EASY_OK;
}

/**
 * easy io callback
 * 用于server端
 */
void *easy_http_server_on_decode(easy_message_t *m)
{
    easy_http_request_t     *p;
    char                    *plast;
    int                     n, size;

    // create http request
    if (m->user_data == NULL && easy_http_request_create(m, HTTP_REQUEST) == EASY_ERROR) {
        easy_error_log("easy_http_request_create failure\n");
        m->status = EASY_ERROR;
        return NULL;
    }

    p = (easy_http_request_t *) m->user_data;
    plast = m->input->pos + p->parsed_byte;

    if ((size = m->input->last - plast) <= 0) {
        return NULL;
    }

    n = http_parser_execute(&p->parser, &easy_http_request_settings, plast, size);

    if (http_parser_has_error(&p->parser) || n < 0) {
        m->status = EASY_ERROR;
        return NULL;
    }

    p->parsed_byte += n;

    // 没读完
    if (!p->message_complete_called) {
        return NULL;
    }

    m->input->pos += (p->parsed_byte + 1);
    m->user_data = NULL;
    p->keep_alive = http_should_keep_alive(&p->parser);

    if (p->keep_alive == 0) {
        m->c->wait_close = 1;
    }

    return p;
}

/**
 * 响应的时候encode, 用于server端
 */
int easy_http_server_on_encode(easy_request_t *r, void *data)
{
    easy_http_request_t     *p;
    easy_buf_t              *b;
    easy_http_header_t      *header;
    int                     i, size;

    p = (easy_http_request_t *)data;

    if (p->is_raw_header == 0) {
        // header length
        if (p->status_line.len == 0)
            easy_buf_string_set(&p->status_line, EASY_HTTP_STATUS_200);

        if (p->content_type.len == 0)
            easy_buf_string_set(&p->content_type, "text/html");

        size = p->status_line.len + p->content_type.len;
        // headers
        easy_http_header_hash_foreach(i, header, &p->headers_out) {
            size += header->name.len + 2;
            size += header->value.len + 2;
        }
        size += 128;

        // body length
        if (p->content_length == -1) {
            p->content_length = 0;
            easy_list_for_each_entry(b, &p->output, node) {
                p->content_length += easy_buf_len(b);
            }
        }

        // concat headers
        if ((b = easy_buf_create(r->ms->pool, size)) == NULL)
            return EASY_ERROR;

        b->last += snprintf(b->last, (b->end - b->last), "HTTP/%d.%d %s%s",
                            p->parser.http_major, p->parser.http_minor,
                            easy_buf_string_ptr(&p->status_line), EASY_HTTP_CRLF);

        // headers
        easy_http_header_hash_foreach(i, header, &p->headers_out) {
            memcpy(b->last, easy_buf_string_ptr(&header->name), header->name.len);
            b->last += header->name.len;
            memcpy(b->last, ": ", 2);
            b->last += 2;

            memcpy(b->last, easy_buf_string_ptr(&header->value), header->value.len);
            b->last += header->value.len;
            memcpy(b->last, EASY_HTTP_CRLF, 2);
            b->last += 2;
        }
        // Content-Type
        b->last += snprintf(b->last, (b->end - b->last),
                            "Content-Type: %s" EASY_HTTP_CRLF, easy_buf_string_ptr(&p->content_type));
        // Content-Length
        b->last += snprintf(b->last, (b->end - b->last),
                            "Content-Length: %" PRIdFAST64 EASY_HTTP_CRLF, p->content_length);
        // keep alive
        b->last += snprintf(b->last, (b->end - b->last),
                            "Connection: %s" EASY_HTTP_CRLF,
                            (p->keep_alive ? "keep-alive" : "close"));
        // header end
        memcpy(b->last, EASY_HTTP_CRLF, 2);
        b->last += 2;

        easy_request_addbuf(r, b);
    }

    easy_request_addbuf_list(r, &p->output);

    return EASY_OK;
}

/**
 * 请求的时候encode, 用于client端
 */
int easy_http_client_on_encode(easy_request_t *r, void *data)
{
    easy_http_packet_t      *p;
    easy_buf_t              *b;
    easy_http_header_t      *header;
    char                    *url, *query;
    int                     i, size, length;

    p = (easy_http_packet_t *)data;

    if (p->is_raw_header == 0) {
        size = p->str_path.len + p->str_query_string.len;
        length = p->post_method ? p->str_query_string.len : 0;
        // headers
        easy_http_header_hash_foreach(i, header, &p->headers_out) {
            size += header->name.len + 2;
            size += header->value.len + 2;
        }
        size += 64;

        // concat headers
        if ((b = easy_buf_create(r->ms->pool, size)) == NULL)
            return EASY_ERROR;

        url = easy_buf_string_ptr(&p->str_path);
        query = easy_buf_string_ptr(&p->str_query_string);

        if (p->post_method) {
            b->last += snprintf(b->last, (b->end - b->last), "POST %s HTTP/1.1%s",
                                (url ? url : "/"), EASY_HTTP_CRLF);
        } else {
            b->last += snprintf(b->last, (b->end - b->last), "GET %s%s%s HTTP/1.1%s",
                                (url ? url : "/"), (query ? "?" : " "),
                                (query ? query : ""), EASY_HTTP_CRLF);
        }

        // headers
        easy_http_header_hash_foreach(i, header, &p->headers_out) {
            memcpy(b->last, easy_buf_string_ptr(&header->name), header->name.len);
            b->last += header->name.len;
            memcpy(b->last, ": ", 2);
            b->last += 2;

            memcpy(b->last, easy_buf_string_ptr(&header->value), header->value.len);
            b->last += header->value.len;
            memcpy(b->last, EASY_HTTP_CRLF, 2);
            b->last += 2;
        }

        // Content-Length
        if (length > 0)
            b->last += snprintf(b->last, (b->end - b->last),
                                "Content-Length: %d" EASY_HTTP_CRLF, length);

        // keep alive
        if (p->keep_alive)
            b->last += snprintf(b->last, (b->end - b->last),
                                "Connection: keep-alive" EASY_HTTP_CRLF);

        // header end
        memcpy(b->last, EASY_HTTP_CRLF, 2);
        b->last += 2;

        // post
        if (length > 0)
            b->last += snprintf(b->last, (b->end - b->last), "%s", (query ? query : ""));

        easy_request_addbuf(r, b);
    }

    easy_request_addbuf_list(r, &p->output);

    return EASY_OK;
}

void *easy_http_client_on_decode(easy_message_t *m)
{
    easy_http_request_t     *p;
    char                    *plast;
    int                     n, size;

    // create http request
    if (m->user_data == NULL && easy_http_request_create(m, HTTP_RESPONSE) == EASY_ERROR) {
        easy_error_log("easy_http_request_create failure\n");
        m->status = EASY_ERROR;
        return NULL;
    }

    p = (easy_http_request_t *) m->user_data;
    plast = m->input->pos + p->parsed_byte;

    if ((size = m->input->last - plast) <= 0) {
        return NULL;
    }

    n = http_parser_execute(&p->parser, &easy_http_response_settings, plast, size);

    if (http_parser_has_error(&p->parser) || n < 0) {
        m->status = EASY_ERROR;
        return NULL;
    }

    p->parsed_byte += n;

    // 没读完
    if (!p->message_complete_called) {
        return NULL;
    }

    m->input->pos += (p->parsed_byte + 1);
    m->user_data = NULL;
    p->keep_alive = http_should_keep_alive(&p->parser);

    if (p->keep_alive == 0) {
        m->c->wait_close = 1;
    }

    return p;
}

/**
 * 新创一个easy_http_request_t
 */
easy_http_packet_t *easy_http_packet_create (easy_pool_t *pool)
{
    return (easy_http_packet_t *) easy_pool_calloc(pool, sizeof(easy_http_packet_t));
}

/**
 * 初始化handler
 */
void easy_http_handler_init(easy_io_handler_pt *handler, easy_io_process_pt *process)
{
    memset(handler, 0, sizeof(easy_io_handler_pt));
    handler->decode = easy_http_server_on_decode;
    handler->encode = easy_http_server_on_encode;
    handler->process = process;
}

//////////////////////////////////////////////////////////////////////////////////////
static inline int easy_htoi(char *ch)
{
    int digit;
    digit = ((ch[0] >= 'A') ? ((ch[0] & 0xdf) - 'A') + 10 : (ch[0] - '0'));
    digit *= 16;
    digit += ((ch[1] >= 'A') ? ((ch[1] & 0xdf) - 'A') + 10 : (ch[1] - '0'));
    return digit;
}

int easy_url_decode(char *str, int len)
{
    char *dest = str;
    char *data = str;

    while (len--) {
        if (*data == '%' && len >= 2 && isxdigit((int) * (data + 1))
                && isxdigit((int) * (data + 2))) {
            *dest = (char) easy_htoi(data + 1);
            data += 2;
            len -= 2;
        } else {
            *dest = *data;
        }

        data++;
        dest++;
    }

    *dest = '\0';
    return dest - str;
}

/**
 * 把header上的string用0结尾
 */
void easy_http_header_string_end(easy_http_request_t *p)
{
    easy_http_header_t      *t;
    int                     n;

    // 把string结尾加0
    easy_http_string_end(&p->str_path);
    easy_http_string_end(&p->str_query_string);
    easy_http_string_end(&p->str_fragment);
    easy_http_header_hash_foreach(n, t, &p->headers_in) {
        easy_http_string_end(&t->name);
        easy_http_string_end(&t->value);
    }
}

/**
 * 把两个path merge起来
 */
int easy_http_merge_path(char *newpath, int len, const char *rootpath, const char *addpath)
{
    const char          *p;
    char                *u, *ue;
    int                 state, size;

    u = newpath;
    ue = u + len - 1;

    if ((size = strlen(rootpath)) >= len)
        return EASY_ERROR;

    memcpy(newpath, rootpath, size);
    newpath += size;
    u = newpath;
    ue = u + (len - size) - 1;

    if (ue > u && size > 0 && *(u - 1) != '/' && *addpath != '/') *u++ = '/';

    // addpath
    p = addpath;
    state = 0;

    while(*p) {
        if (u == ue || u < newpath)
            return EASY_ERROR;

        *u ++ = *p;

        if (*p == '/') {
            if (state) u -= state;

            if (state == 5) {
                while(u >= newpath) {
                    if(*u == '/') {
                        u++;
                        break;
                    }

                    u --;
                }
            }

            state = 1;
        } else if (state && *p == '.')
            state = (state == 5 ? 0 : (state == 2 ? 5 : 2));
        else
            state = 0;

        p ++;
    }

    *u = '\0';
    return EASY_OK;
}

int easy_http_request_printf(easy_http_request_t *r, const char *fmt, ...)
{
    int             len;
    char            buffer[EASY_POOL_PAGE_SIZE];
    easy_buf_t      *b;

    va_list args;
    va_start(args, fmt);
    len = vsnprintf(buffer, EASY_POOL_PAGE_SIZE, fmt, args);
    va_end(args);
    b = easy_buf_check_write_space(r->m->pool, &r->output, len);
    memcpy(b->last, buffer, len);
    b->last += len;
    return len;
}
