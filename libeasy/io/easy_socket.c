/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#include "easy_socket.h"
#include "easy_io.h"
#include <easy_inet.h>
#include <easy_string.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>

static int easy_socket_chain_writev(int fd, easy_list_t *l, struct iovec *iovs, int cnt, int *again);
static int easy_socket_sendfile(int fd, easy_file_buf_t *fb, int *again);

/**
 * 打开监听端口
 */
int easy_socket_listen(easy_addr_t *address)
{
    int                 fd = -1;
    struct sockaddr_in  addr;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        easy_trace_log("create socket error.\n");
        goto error_exit;
    }

    easy_socket_non_blocking(fd);
    easy_socket_set_opt(fd, SO_REUSEADDR, 1);
    easy_socket_set_tcpopt(fd, TCP_DEFER_ACCEPT, 1);
    memset(&addr, 0, sizeof(struct sockaddr_in));
    memcpy(&addr, address, sizeof(uint64_t));

    if (bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) < 0) {
        easy_trace_log("bind socket error: %d\n", errno);
        goto error_exit;
    }

    if (listen(fd, 1024) < 0) {
        easy_trace_log("listen error. %d\n", errno);
        goto error_exit;
    }

    return fd;

error_exit:

    if (fd >= 0)
        close(fd);

    return -1;
}

/**
 * 把buf_chain_t上的内容通过writev写到socket上
 */
#define EASY_SOCKET_RET_CHECK(ret, size, again) if (ret<0) return ret; else size += ret; if (again) return size;
int easy_socket_write(int fd, easy_list_t *l)
{
    easy_buf_t      *b, *b1;
    easy_file_buf_t *fb;
    struct          iovec iovs[EASY_IOV_MAX];
    int             sended, size, cnt, ret, wbyte, again;

    wbyte = cnt = sended = again = 0;

    // foreach
    easy_list_for_each_entry_safe(b, b1, l, node) {
        // sendfile
        if ((b->flags & EASY_BUF_FILE)) {
            // 先writev出去
            if (cnt > 0) {
                ret = easy_socket_chain_writev(fd, l, iovs, cnt, &again);
                EASY_SOCKET_RET_CHECK(ret, wbyte, again);
                cnt = 0;
            }

            fb = (easy_file_buf_t *)b;
            sended += fb->count;
            ret = easy_socket_sendfile(fd, fb, &again);
            EASY_SOCKET_RET_CHECK(ret, wbyte, again);
        } else {
            size = b->last - b->pos;
            iovs[cnt].iov_base = b->pos;
            iovs[cnt].iov_len = size;
            cnt ++;
            sended += size;
        }

        // 跳出
        if (cnt >= EASY_IOV_MAX || sended >= EASY_IOV_SIZE)
            break;
    }

    // writev
    if (cnt > 0) {
        ret = easy_socket_chain_writev(fd, l, iovs, cnt, &again);
        EASY_SOCKET_RET_CHECK(ret, wbyte, again);
    }

    return wbyte;
}

/**
 * writev
 */
static int easy_socket_chain_writev(int fd, easy_list_t *l, struct iovec *iovs, int cnt, int *again)
{
    int             ret, sended, size;
    easy_buf_t      *b, *b1;

    do {
        ret = writev(fd, iovs, cnt);
    } while(ret == -1 && errno == EINTR);

    // 结果处理
    if (ret >= 0) {
        sended = ret;
        easy_debug_log("writev: %d, fd: %d\n", ret, fd);
        easy_atomic_add(&EASY_IOTH_SELF->eio->send_byte, ret);

        easy_list_for_each_entry_safe(b, b1, l, node) {
            size = b->last - b->pos;

            if (easy_log_level >= EASY_LOG_TRACE) {
                char btmp[64];
                easy_trace_log("write: %d,%d => %s", size, sended, easy_string_tohex(b->pos, size, btmp, 64));
            }

            b->pos += sended;
            sended -= size;

            if (sended >= 0) {
                cnt --;
                easy_buf_destroy(b);
            }

            if (sended <= 0)
                break;
        }
        *again = (cnt > 0);
    } else {
        ret = ((errno == EAGAIN) ? EASY_AGAIN : EASY_ERROR);
    }

    return ret;
}

/**
 * sendfile
 */
static int easy_socket_sendfile(int fd, easy_file_buf_t *fb, int *again)
{
    int ret;

    do {
        ret = sendfile(fd, fb->fd, (off_t *)&fb->offset, fb->count);
    } while(ret == -1 && errno == EINTR);

    // 结果处理
    if (ret >= 0) {
        easy_debug_log("sendfile: %d, fd: %d\n", ret, fd);
        easy_atomic_add(&EASY_IOTH_SELF->eio->send_byte, ret);

        if (ret < fb->count) {
            fb->count -= ret;
            *again = 1;
        } else {
            easy_buf_destroy((easy_buf_t *)fb);
        }
    } else {
        ret = ((errno == EAGAIN) ? EASY_AGAIN : EASY_ERROR);
    }

    return ret;
}

// 非阻塞
int easy_socket_non_blocking(int fd)
{
    int     flags = 1;
    return ioctl(fd, FIONBIO, &flags);
}

// TCP
int easy_socket_set_tcpopt(int fd, int option, int value)
{
    return setsockopt(fd, IPPROTO_TCP, option, (const void *) &value, sizeof(value));
}

// SOCKET
int easy_socket_set_opt(int fd, int option, int value)
{
    return setsockopt(fd, SOL_SOCKET, option, (void *)&value, sizeof(value));
}
