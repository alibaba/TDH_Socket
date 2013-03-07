/*
 * Copyright(C) 2011-2012 Alibaba Group Holding Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * tdh_socket_request_thread.cpp
 *
 *  Created on: 2011-11-25
 *      Author: wentong
 */
#include "tdh_socket_request_thread.hpp"
#include "tdh_socket_connection_context.hpp"
#include "tdh_socket_encode_response.hpp"
#include "tdh_socket_define.hpp"
#include "tdh_socket_protocol.hpp"
#include "tdh_socket_config.hpp"
#include "debug_util.hpp"

namespace taobao {

/**
 * 唤醒io线程
 */
static void tdhs_request_wakeup_ioth(easy_io_t *eio, char *ioth_flag,
		easy_list_t *ioth_list) {
	int i;
	easy_io_thread_t *ioth;

	for (i = 0; i < eio->io_thread_count; i++) {
		if (!ioth_flag[i])
			continue;

		ioth_flag[i] = 0;
		ioth = (easy_io_thread_t *) easy_thread_pool_index(eio->io_thread_pool,
				i);

		// dispatch to ioth
		easy_spin_lock(&ioth->thread_lock);
		easy_list_join(&ioth_list[ioth->idx], &ioth->request_list);
		ev_async_send(ioth->loop, &ioth->thread_watcher);
		easy_spin_unlock(&ioth->thread_lock);
	}
}

static void tdhs_request_doreq_for_read(easy_request_thread_t *th,
		easy_list_t *request_list) {
	easy_debug_log("TDHS:tdhs_request_doreq_for_read start!");
	tdhs_dbcontext_i *dbcontext = (tdhs_dbcontext_i*) th->args;
	tb_assert(dbcontext!=NULL);
	easy_request_t *r, *r2;
	easy_connection_t *c;
	tdhs_packet_t *packet;
	int cnt;

	cnt = 0;
	char ioth_flag[th->eio->io_thread_count];
	easy_list_t ioth_list[th->eio->io_thread_count];
	memset(ioth_flag, 0, sizeof(ioth_flag));
	unsigned int request_num = 0;
	//decode and prepare open/lock table
	easy_list_for_each_entry_safe(r, r2, request_list, request_list_node) {
		packet = (tdhs_packet_t*) ((r->ipacket));
		tb_assert(packet != NULL);
		while (packet) {
			dbcontext->open_table(packet->req);
			packet = packet->next;
		}
		request_num++;
	}
	if (request_num == 0) {
		dbcontext->set_thd_info(request_num);
		easy_debug_log("TDHS:tdhs_request_doreq_for_read no request!");
		return;
	}
	dbcontext->lock_table();

	// process
	easy_list_for_each_entry_safe(r, r2, request_list, request_list_node) {
		c = r->ms->c;
		easy_list_del(&r->request_list_node);

		// 处理
		if (c->status != EASY_CONN_CLOSE) {
			int ret = (th->process)(r, th->args);
			if (ret == EASY_ASYNC) {
				//此时request已经被destory了
				continue;
			}
			r->retcode = ret;
		} else {
			r->retcode = EASY_ERROR;
		}

		if (!ioth_flag[c->ioth->idx])
			easy_list_init(&ioth_list[c->ioth->idx]);

		easy_list_add_tail(&r->request_list_node, &ioth_list[c->ioth->idx]);
		ioth_flag[c->ioth->idx] = 1;

		if (++cnt >= 32) {
			tdhs_request_wakeup_ioth(th->eio, ioth_flag, ioth_list);
			cnt = 0;
		}
	}

	if (cnt > 0)
		tdhs_request_wakeup_ioth(th->eio, ioth_flag, ioth_list);

	dbcontext->unlock_table();
	dbcontext->close_table();
	dbcontext->set_thd_info(request_num);
	easy_debug_log("TDHS:tdhs_request_doreq_for_read end!");
}

static void tdhs_request_doreq_for_write(easy_request_thread_t *th,
		easy_list_t *request_list) {
	easy_debug_log("TDHS:tdhs_request_doreq_for_write start!");
	easy_list_t done_request;
	easy_list_init(&done_request);
	tdhs_dbcontext_i *dbcontext = (tdhs_dbcontext_i*) th->args;
	tb_assert(dbcontext!=NULL);
	easy_request_t *r, *r2;
	easy_connection_t *c;
	tdhs_packet_t *packet;
	int cnt;

	cnt = 0;
	char ioth_flag[th->eio->io_thread_count];
	easy_list_t ioth_list[th->eio->io_thread_count];
	memset(ioth_flag, 0, sizeof(ioth_flag));
	unsigned int request_num = 0;
	dbcontext->set_group_commit(tdhs_group_commit);
	//decode and prepare open/lock table
	easy_list_for_each_entry_safe(r, r2, request_list, request_list_node) {
		packet = (tdhs_packet_t*) ((r->ipacket));
		tb_assert(packet != NULL);
		while (packet) {
			dbcontext->open_table(packet->req);
			packet = packet->next;
		}
		request_num++;
	}
	if (request_num == 0) {
		dbcontext->set_thd_info(request_num);
		easy_debug_log("TDHS:tdhs_request_doreq_for_write no request!");
		return;
	}
	dbcontext->lock_table();

	//do request
	easy_list_for_each_entry_safe(r, r2, request_list, request_list_node) {
		c = r->ms->c;
		easy_list_del(&r->request_list_node);

		// 处理
		if (c->status != EASY_CONN_CLOSE) {
			int ret = (th->process)(r, th->args);
			if (ret == EASY_ASYNC) {
				//此时request已经被destory了
				continue;
			}
			r->retcode = ret;
		} else {
			r->retcode = EASY_ERROR;
		}
		easy_list_add_tail(&r->request_list_node, &done_request);
	}

	int commit_ret = dbcontext->unlock_table();
	dbcontext->close_table();
	dbcontext->set_thd_info(request_num);

	// do response
	easy_list_for_each_entry_safe(r, r2, &done_request, request_list_node) {
		c = r->ms->c;
		easy_list_del(&r->request_list_node);
		if (commit_ret != EASY_OK) {
			//commit失败替换返回信息
			if (r->retcode == EASY_OK && r->opacket) {
				tdhs_packet_t * rp = (tdhs_packet_t *) r->opacket;
				while (rp) {
					if (rp->command_id_or_response_code >= 200
							&& rp->command_id_or_response_code < 300
							&& rp->command_id_or_response_code
									!= CLIENT_STATUS_MULTI_STATUS) {
						//只对正确的结果进行返回. BATCH返回的头也要做过滤
						r->retcode = tdhs_response_error(rp,
								CLIENT_STATUS_SERVER_ERROR,
								CLIENT_ERROR_CODE_FAILED_TO_COMMIT);
					}
					rp = rp->next;
				}
			}
		}

		if (!ioth_flag[c->ioth->idx])
			easy_list_init(&ioth_list[c->ioth->idx]);

		easy_list_add_tail(&r->request_list_node, &ioth_list[c->ioth->idx]);
		ioth_flag[c->ioth->idx] = 1;

		if (++cnt >= 32) {
			tdhs_request_wakeup_ioth(th->eio, ioth_flag, ioth_list);
			cnt = 0;
		}
	}
	if (cnt > 0)
		tdhs_request_wakeup_ioth(th->eio, ioth_flag, ioth_list);
	easy_debug_log("TDHS:tdhs_request_doreq_for_write end!");
}

static void tdhs_request_on_wakeup(struct ev_loop *loop, ev_async *w,
		int revents) {
	easy_debug_log("TDHS:tdhs_request_on_wakeup");
	easy_request_thread_t *th;
	easy_list_t request_list;
	easy_list_t session_list;

	th = (easy_request_thread_t *) w->data;

	tdhs_dbcontext_i *dbcontext = (tdhs_dbcontext_i*) th->args;
	tb_assert(dbcontext!=NULL);
	if (tdhs_cache_table_on == 0) {
		easy_debug_log("TDHS:tdhs_cache_table_on is off,close_cached_table");
		dbcontext->close_cached_table();
	}


	if (dbcontext->need_write()) {
		// 取回list
		int limits = tdhs_group_commit_limits;
		easy_spin_lock(&th->thread_lock);
		if (limits > 0 && th->task_list_count > limits) {
			easy_request_t *r, *r2;
			easy_list_init(&request_list);
			th->task_list_count -= limits;
			easy_list_for_each_entry_safe(r, r2, &th->task_list, request_list_node)
			{
				easy_list_del(&r->request_list_node);
				easy_list_add_tail(&r->request_list_node, &request_list);
				if (--limits <= 0) {
					break;
				}
			}
		} else {
			th->task_list_count = 0;
			easy_list_movelist(&th->task_list, &request_list);
		}
		easy_list_movelist(&th->session_list, &session_list);
		easy_spin_unlock(&th->thread_lock);

		tdhs_request_doreq_for_write(th, &request_list);
	} else {
		// 取回list
		easy_spin_lock(&th->thread_lock);
		th->task_list_count = 0;
		easy_list_movelist(&th->task_list, &request_list);
		easy_list_movelist(&th->session_list, &session_list);
		easy_spin_unlock(&th->thread_lock);

		tdhs_request_doreq_for_read(th, &request_list);
	}

    if (dbcontext->need_close_table()) {
        easy_debug_log("TDHS:need close table ,close_cached_table");
        dbcontext->close_cached_table();
        dbcontext->set_need_close_table(false);
    }
}

// 自己定义start
easy_thread_pool_t *tdhs_thread_pool_create_ex(easy_io_t *eio, int cnt,
		easy_baseth_on_start_pt *start, easy_request_process_pt *cb,
		void *args) {
	easy_thread_pool_t *tp;
	easy_request_thread_t *rth;

	if ((tp = easy_baseth_pool_create(eio, cnt, sizeof(easy_request_thread_t)))
			== NULL)
		return NULL;

	// 初始化线程池
	easy_thread_pool_for_each(rth, tp, 0) {
		easy_baseth_init(rth, tp, start, tdhs_request_on_wakeup);

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

void wakeup_request_thread(easy_thread_pool_t * thread_pool) {
	easy_request_thread_t *rth;
	if (thread_pool == NULL) {
		return;
	}
	for (int i = 0; i < thread_pool->thread_count; i++) {
		rth = (easy_request_thread_t *) easy_thread_pool_index(thread_pool, i);
		if (rth != NULL) {
			easy_spin_lock(&rth->thread_lock);
			ev_async_send(rth->loop, &rth->thread_watcher);
			easy_spin_unlock(&rth->thread_lock);
		}
	}
}

} // namespace taobao
