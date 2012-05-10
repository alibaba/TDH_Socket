/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_IO_STRUCT_H_
#define EASY_IO_STRUCT_H_

#include <easy_define.h>

/**
 * IO结构定义
 */

EASY_CPP_START

#define EV_STANDALONE    1
#define EV_USE_MONOTONIC 0
#include "ev.h"
#include <easy_pool.h>
#include <easy_buf.h>
#include <easy_list.h>
#include <easy_atomic.h>
#include <easy_hash.h>
#include <easy_uthread.h>
#include <easy_inet.h>
#include <easy_array.h>

///// define
#define EASY_MAX_THREAD_CNT         64
#define EASY_IOTH_DOING_REQ_CNT     8192
#define EASY_CONN_DOING_REQ_CNT     1024
#define EASY_MAX_CLIENT_CNT         65536

#define EASY_EVENT_READ             1
#define EASY_EVENT_WRITE            2
#define EASY_EVENT_TIMEOUT          4

#define EASY_TYPE_SERVER            0
#define EASY_TYPE_CLIENT            1

#define EASY_TYPE_MESSAGE           1
#define EASY_TYPE_SESSION           2

#define EASY_CONN_OK                0
#define EASY_CONN_CONNECTING        1
#define EASY_CONN_AUTO_CONN         2
#define EASY_CONN_CLOSE             3

#define EASY_IO_BUFFER_SIZE         (8192-sizeof(easy_pool_t))
#define EASY_MESG_READ_AGAIN        1
#define EASY_MESG_WRITE_AGAIN       2
#define EASY_MESG_DESTROY           3

#define EASY_IOV_MAX                256
#define EASY_IOV_SIZE               262144

#define EASY_FILE_READ              1
#define EASY_FILE_WRITE             2
#define EASY_FILE_SENDFILE          3
#define EASY_FILE_WILLNEED          4

// async + spinlock
#define EASY_BASETH_DEFINE                          \
    easy_baseth_on_start_pt         *on_start;      \
    pthread_t                       tid;            \
    int                             idx, iot;       \
    struct ev_loop                  *loop;          \
    ev_async                        thread_watcher; \
    easy_atomic_t                   thread_lock;    \
    easy_io_t                       *eio;

///// typedef
typedef struct easy_io_thread_t easy_io_thread_t;
typedef struct easy_request_thread_t easy_request_thread_t;
typedef struct easy_message_t easy_message_t;
typedef struct easy_request_t easy_request_t;
typedef struct easy_connection_t easy_connection_t;
typedef struct easy_message_session_t easy_message_session_t;
typedef struct easy_session_t easy_session_t;
typedef struct easy_listen_t easy_listen_t;
typedef struct easy_client_t easy_client_t;
typedef struct easy_io_t easy_io_t;
typedef struct easy_io_handler_pt easy_io_handler_pt;
typedef struct easy_io_stat_t easy_io_stat_t;
typedef struct easy_baseth_t easy_baseth_t;
typedef struct easy_thread_pool_t easy_thread_pool_t;
typedef struct easy_client_wait_t easy_client_wait_t;
typedef struct easy_file_task_t easy_file_task_t;

typedef int (easy_io_process_pt)(easy_request_t *r);
typedef int (easy_io_cleanup_pt)(easy_request_t *r, void *apacket);
typedef int (easy_request_process_pt)(easy_request_t *r, void *args);
typedef void (easy_io_stat_process_pt)(easy_io_stat_t *iostat);
typedef void (easy_io_uthread_start_pt)(void *args);
typedef void *(easy_baseth_on_start_pt)(void *args);
typedef void (easy_baseth_on_wakeup_pt)(struct ev_loop *loop, ev_async *w, int revents);

struct easy_io_handler_pt {
    void               *(*decode)(easy_message_t *m);
    int                 (*encode)(easy_request_t *r, void *packet);
    easy_io_process_pt  *process;
    int                 (*batch_process)(easy_message_t *m);
    easy_io_cleanup_pt  *cleanup;
    uint64_t            (*get_packet_id)(easy_connection_t *c, void *packet);
    int                 (*on_connect) (easy_connection_t *c);
    int                 (*on_disconnect) (easy_connection_t *c);
    int                 (*new_packet) (easy_connection_t *c);
    int                 (*on_idle) (easy_connection_t *c);
    void                *user_data, *user_data2;
    int                 is_uthread;
};

// 处理IO的线程
struct easy_io_thread_t {
    EASY_BASETH_DEFINE

    // queue
    easy_list_t             conn_list;
    easy_list_t             session_list;
    easy_list_t             request_list;

    // listen watcher
    ev_timer                listen_watcher;
    easy_io_uthread_start_pt *on_utstart;
    void                    *ut_args;

    // client list
    easy_hash_t             *client_list;
    easy_array_t            *client_array;

    // connected list
    easy_list_t             connected_list;
    easy_atomic32_t         doing_request_count;
    uint64_t                done_request_count;
};

// 处理任务的线程
struct easy_request_thread_t {
    EASY_BASETH_DEFINE

    // queue
    int                     task_list_count;
    easy_list_t             task_list;
    easy_list_t             session_list;

    easy_request_process_pt *process;
    void                    *args;
};

// 保存client
struct easy_client_t {
    easy_addr_t             addr;
    easy_connection_t       *c;
    easy_io_handler_pt      *handler;
    int                     timeout, ref;
    easy_hash_list_t        client_list_node;
    void                    *user_data;
};

// 对应一个SOCKET连接
struct easy_connection_t {
    struct ev_loop          *loop;
    easy_pool_t             *pool;
    easy_io_thread_t        *ioth;
    easy_connection_t       *next;
    easy_list_t             conn_list_node;

    // file description
    uint32_t                default_message_len;
    int                     reconn_time, reconn_fail;
    int                     idle_time;
    int                     fd;
    easy_addr_t             addr;

    ev_io                   read_watcher;
    ev_io                   write_watcher;
    ev_timer                timeout_watcher;
    easy_list_t             message_list;

    easy_list_t             output;
    easy_io_handler_pt      *handler;
    easy_client_t           *client;
    easy_list_t             session_list;
    easy_hash_t             *send_queue;
    void                    *user_data;

    uint32_t                status : 4;
    uint32_t                event_status : 4;
    uint32_t                type : 1;
    uint32_t                async_conn : 1;
    uint32_t                conn_has_error : 1;
    uint32_t                tcp_cork_flag : 1;
    uint32_t                tcp_nodelay_flag : 1;
    uint32_t                wait_close : 1;
    uint32_t                need_redispatch : 1;
    uint32_t                read_eof : 1;
    uint32_t                auto_reconn : 1;

    uint32_t                doing_request_count;
    uint64_t                done_request_count;
    ev_tstamp               start_time, sw_time, last_time;
    easy_uthread_t          *uthread;   //user thread
};

// ipacket放进来的包, opacket及出去的包
struct easy_request_t {
    easy_message_session_t  *ms;

    easy_list_t             request_list_node;
    int                     retcode;
    void                    *ipacket;
    void                    *opacket;
    void                    *args;
};

#define EASY_MESSAGE_SESSION_HEADER \
    easy_connection_t       *c;     \
    easy_pool_t             *pool;  \
    uint8_t                 type:4; \
    uint8_t                 async:4;\
    int8_t                  status;

struct easy_message_session_t {
    EASY_MESSAGE_SESSION_HEADER
};

// 用于接收, 一个或多个easy_request_t
struct easy_message_t {
    EASY_MESSAGE_SESSION_HEADER
    uint16_t                recycle_cnt;

    easy_buf_t              *input;
    easy_list_t             message_list_node;
    easy_list_t             request_list;
    easy_list_t             request_done_list;
    int                     request_list_count;
    int                     next_read_len;

    void                    *user_data;
};

// 用于发送, 只带一个easy_request_t
struct easy_session_t {
    EASY_MESSAGE_SESSION_HEADER;
    int16_t                 error;
    int                     timeout;
    ev_timer                timeout_watcher;

    easy_list_t             session_list_node;
    easy_hash_list_t        send_queue_hash;
    easy_list_t             send_queue_list;
    easy_io_process_pt     *process;
    easy_io_cleanup_pt     *cleanup;
    easy_addr_t             addr;
    easy_list_t            *nextb;

    uint64_t                packet_id;
    void                    *thread_ptr;
    easy_request_t          r;
    char                    data[0];
};

// 监听列表
struct easy_listen_t {
    int                     fd;
    int8_t                  cur, old;
    easy_addr_t             addr;
    easy_io_handler_pt      *handler;

    easy_atomic_t           listen_lock;
    easy_io_thread_t        *curr_ioth;
    easy_io_thread_t        *old_ioth;

    easy_listen_t           *next;
    ev_io                   read_watcher[0];
};

// 用于统计处理速度
struct easy_io_stat_t {
    int64_t                 last_cnt;
    ev_tstamp               last_time;
    double                  last_speed;
    double                  total_speed;
    easy_io_stat_process_pt *process;
    easy_io_t               *eio;
};

// easy_io对象
struct easy_io_t {
    easy_pool_t             *pool;
    easy_list_t             eio_list_node;
    easy_atomic_t           lock;

    easy_listen_t           *listen;
    int                     io_thread_count;
    easy_thread_pool_t      *io_thread_pool;
    easy_thread_pool_t      *thread_pool;
    void                    *user_data;
    easy_list_t             thread_pool_list;

    // flags
    uint32_t                stoped : 1;
    uint32_t                started : 1;
    uint32_t                tcp_cork : 1;
    uint32_t                tcp_nodelay : 1;
    uint32_t                listen_all : 1;
    uint32_t                uthread_enable : 1;
    uint32_t                affinity_enable : 1;
    uint32_t                no_redispatch : 1;

    ev_tstamp               start_time;
    easy_atomic_t           send_byte;
    easy_atomic_t           recv_byte;
};

// base thread
struct easy_baseth_t {
    EASY_BASETH_DEFINE
};

struct easy_thread_pool_t {
    int                     thread_count;
    int                     member_size;
    easy_atomic32_t         last_number;
    easy_list_t             list_node;
    easy_thread_pool_t      *next;
    char                    *last;
    char                    data[0];
};

struct easy_client_wait_t {
    int                     done_count;
    pthread_mutex_t         mutex;
    pthread_cond_t          cond;
    easy_list_t             session_list;
    easy_list_t             next_list;
};

struct easy_file_task_t {
    int                     fd;
    int                     bufsize;
    char                    *buffer;
    int64_t                 offset;
    int64_t                 count;
    easy_buf_t              *b;
    void                    *args;
};

EASY_CPP_END

#endif
