/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#include "easy_io.h"
#include "easy_request.h"
#include "easy_message.h"
#include "easy_connection.h"
#include "easy_file.h"

static void easy_request_on_wakeup(struct ev_loop *loop, ev_async *w, int revents);
static void easy_request_wakeup_ioth(easy_io_t *eio, char *ioth_flag, easy_list_t *ioth_list);
static void easy_request_doreq(easy_request_thread_t *th, easy_list_t *request_list);
static void easy_request_dosess(easy_request_thread_t *th, easy_list_t *session_list);

/**
 * 对request回复响应
 *
 * @param packet对象
 */
int easy_request_do_reply(easy_request_t *r)
{
    easy_connection_t       *c;
    easy_message_t          *m;

    // encode
    m = (easy_message_t *)r->ms;
    c = m->c;

    if (c->ioth->tid != pthread_self()) {
        easy_fatal_log("not run at other thread: %lx <> %lx\n", r, pthread_self(), c->ioth->tid);
        return EASY_ERROR;
    }

    if (c->type == EASY_TYPE_CLIENT)
        return EASY_OK;

    r->retcode = EASY_OK;
    easy_list_del(&r->request_list_node);

    if (easy_connection_request_done(r) == EASY_OK) {
        // 所有的request都有reply了,一起才响应
        if (easy_connection_write_socket(c) == EASY_ABORT) {
            easy_connection_destroy(c);
            return EASY_ABORT;
        }

        if (m->request_list_count == 0 && m->status != EASY_MESG_READ_AGAIN) {
            easy_message_destroy(m, 1);
            return EASY_BREAK;
        }
    }

    return EASY_OK;
}

/**
 * push到c->output上
 */
void easy_request_addbuf(easy_request_t *r, easy_buf_t *b)
{
    easy_message_session_t *ms = r->ms;

    if (ms->type == EASY_TYPE_MESSAGE) {
        easy_atomic_inc(&ms->pool->ref);
        easy_buf_set_cleanup(b, easy_message_cleanup, ms);
    } else {
        easy_session_t *s = (easy_session_t *)ms;

        if (s->nextb == NULL) s->nextb = &b->node;
    }

    easy_list_add_tail(&b->node, &ms->c->output);
}

/**
 * 加list到c->output上
 */
void easy_request_addbuf_list(easy_request_t *r, easy_list_t *list)
{
    easy_buf_t              *b;
    easy_message_session_t  *ms = r->ms;

    // 是否为空
    if (easy_list_empty(list))
        return;

    if (ms->type == EASY_TYPE_MESSAGE) {
        // 在没写回去的时候
        b = easy_list_get_last(list, easy_buf_t, node);
        easy_atomic_inc(&ms->pool->ref);
        easy_buf_set_cleanup(b, easy_message_cleanup, ms);
    } else {
        // 在超时的时间用到
        easy_session_t *s = (easy_session_t *)ms;
        b = easy_list_get_first(list, easy_buf_t, node);

        if (s->nextb == NULL && b) s->nextb = &b->node;
    }

    easy_list_join(list, &ms->c->output);
}

/**
 * destroy掉easy_request_t对象
 */
void easy_request_server_done(easy_request_t *r)
{
    easy_connection_t   *c = r->ms->c;

    if (c->type == EASY_TYPE_SERVER && c->handler->cleanup) {
        (c->handler->cleanup)(r, NULL);
    }
}

void easy_request_client_done(easy_request_t *r)
{
    easy_connection_t   *c = r->ms->c;
    c->doing_request_count --;
    c->done_request_count ++;
    easy_atomic32_dec(&c->ioth->doing_request_count);
}

// request thread pool
easy_thread_pool_t *easy_thread_pool_create(easy_io_t *eio, int cnt, easy_request_process_pt *cb, void *args)
{
    return easy_thread_pool_create_ex(eio, cnt, easy_baseth_on_start, cb, args);
}

// 自己定义start
easy_thread_pool_t *easy_thread_pool_create_ex(easy_io_t *eio, int cnt,
        easy_baseth_on_start_pt *start, easy_request_process_pt *cb, void *args)
{
    easy_thread_pool_t      *tp;
    easy_request_thread_t   *rth;

    if ((tp = easy_baseth_pool_create(eio, cnt, sizeof(easy_request_thread_t))) == NULL)
        return NULL;

    // 初始化线程池
    easy_thread_pool_for_each(rth, tp, 0) {
        easy_baseth_init(rth, tp, start, easy_request_on_wakeup);

        rth->process = cb;
        rth->args = args;
        easy_list_init(&rth->task_list);
        easy_list_init(&rth->session_list);
    }

    // join
    tp->next = eio->thread_pool;
    eio->thread_pool = tp;

    return tp;
}

int easy_thread_pool_push(easy_thread_pool_t *tp, easy_request_t *r, uint64_t hv)
{
    easy_request_thread_t   *rth;

    // dispatch
    if (hv == 0) hv = easy_hash_key((long)r->ms->c);

    rth = (easy_request_thread_t *)easy_thread_pool_hash(tp, hv);
    easy_list_del(&r->request_list_node);

    // 引用次数
    r->retcode = EASY_AGAIN;
    r->ms->c->pool->ref ++;
    easy_atomic_inc(&r->ms->pool->ref);
    easy_pool_set_lock(r->ms->pool);

    easy_spin_lock(&rth->thread_lock);
    easy_list_add_tail(&r->request_list_node, &rth->task_list);
    rth->task_list_count ++;
    ev_async_send(rth->loop, &rth->thread_watcher);
    easy_spin_unlock(&rth->thread_lock);

    return EASY_OK;
}

int easy_thread_pool_push_message(easy_thread_pool_t *tp, easy_message_t *m, uint64_t hv)
{
    easy_request_thread_t   *rth;

    // dispatch
    if (hv == 0) hv = easy_hash_key((long)m->c);

    rth = (easy_request_thread_t *)easy_thread_pool_hash(tp, hv);

    // 引用次数
    m->c->pool->ref += m->request_list_count;
    easy_atomic_add(&m->pool->ref, m->request_list_count);
    easy_pool_set_lock(m->pool);

    easy_spin_lock(&rth->thread_lock);
    easy_list_join(&m->request_list, &rth->task_list);
    rth->task_list_count += m->request_list_count;
    ev_async_send(rth->loop, &rth->thread_watcher);
    easy_spin_unlock(&rth->thread_lock);

    easy_list_init(&m->request_list);

    return EASY_OK;
}

/**
 * push session
 */
int easy_thread_pool_push_session(easy_thread_pool_t *tp, easy_session_t *s, uint64_t hv)
{
    easy_request_thread_t   *rth;

    // choice
    if (hv == 0) hv = easy_hash_key((long)s);

    rth = (easy_request_thread_t *)easy_thread_pool_hash(tp, hv);

    easy_spin_lock(&rth->thread_lock);
    easy_list_add_tail(&s->session_list_node, &rth->session_list);
    ev_async_send(rth->loop, &rth->thread_watcher);
    easy_spin_unlock(&rth->thread_lock);

    return EASY_OK;
}

/**
 * WORK线程的回调程序
 */
static void easy_request_on_wakeup(struct ev_loop *loop, ev_async *w, int revents)
{
    easy_request_thread_t       *th;
    easy_list_t                 request_list;
    easy_list_t                 session_list;

    th = (easy_request_thread_t *) w->data;

    // 取回list
    easy_spin_lock(&th->thread_lock);
    th->task_list_count = 0;
    easy_list_movelist(&th->task_list, &request_list);
    easy_list_movelist(&th->session_list, &session_list);
    easy_spin_unlock(&th->thread_lock);

    easy_request_doreq(th, &request_list);
    easy_request_dosess(th, &session_list);
}

static void easy_request_doreq(easy_request_thread_t *th, easy_list_t *request_list)
{
    easy_request_t              *r, *r2;
    easy_connection_t           *c;
    int                         cnt;

    cnt = 0;
    char ioth_flag[th->eio->io_thread_count];
    easy_list_t ioth_list[th->eio->io_thread_count];
    memset(ioth_flag, 0, sizeof(ioth_flag));

    // process
    easy_list_for_each_entry_safe(r, r2, request_list, request_list_node) {
        c = r->ms->c;
        easy_list_del(&r->request_list_node);

        // 处理
        if (c->status != EASY_CONN_CLOSE) {
            r->retcode = (th->process)(r, th->args);
        } else {
            r->retcode = EASY_ERROR;
        }

        if (!ioth_flag[c->ioth->idx])
            easy_list_init(&ioth_list[c->ioth->idx]);

        easy_list_add_tail(&r->request_list_node, &ioth_list[c->ioth->idx]);
        ioth_flag[c->ioth->idx] = 1;

        if (++ cnt >= 32) {
            easy_request_wakeup_ioth(th->eio, ioth_flag, ioth_list);
            cnt = 0;
        }
    }

    if (cnt > 0) {
        easy_request_wakeup_ioth(th->eio, ioth_flag, ioth_list);
    }
}

static void easy_request_dosess(easy_request_thread_t *th, easy_list_t *session_list)
{
    easy_session_t              *s, *s2;

    // process
    easy_list_for_each_entry_safe(s, s2, session_list, session_list_node) {
        easy_list_del(&s->session_list_node);

        if ((th->process)(&s->r, th->args) != EASY_AGAIN) {
            easy_session_destroy(s);
        }
    }
}

/**
 * 唤醒io线程
 */
static void easy_request_wakeup_ioth(easy_io_t *eio, char *ioth_flag, easy_list_t *ioth_list)
{
    int                 i;
    easy_io_thread_t    *ioth;

    for(i = 0; i < eio->io_thread_count; i++) {
        if (!ioth_flag[i])
            continue;

        ioth_flag[i] = 0;
        ioth = (easy_io_thread_t *)easy_thread_pool_index(eio->io_thread_pool, i);

        // dispatch to ioth
        easy_spin_lock(&ioth->thread_lock);
        easy_list_join(&ioth_list[ioth->idx], &ioth->request_list);
        ev_async_send(ioth->loop, &ioth->thread_watcher);
        easy_spin_unlock(&ioth->thread_lock);
    }
}

/**
 * 重新wakeup
 */
void easy_request_wakeup(easy_request_t *r)
{
    easy_io_thread_t *ioth = r->ms->c->ioth;

    easy_spin_lock(&ioth->thread_lock);
    easy_list_add_tail(&r->request_list_node, &ioth->request_list);
    ev_async_send(ioth->loop, &ioth->thread_watcher);
    easy_spin_unlock(&ioth->thread_lock);
}
