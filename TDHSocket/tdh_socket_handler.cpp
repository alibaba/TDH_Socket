/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_handler.cpp
 *
 *  Created on: 2011-9-2
 *      Author: wentong
 */
#include "tdh_socket_define.hpp"
#include "tdh_socket_time.hpp"
#include "tdh_socket_handler.hpp"
#include "tdh_socket_protocol.hpp"
#include "tdh_socket_connection_context.hpp"
#include "tdh_socket_dbcontext.hpp"
#include "tdh_socket_share.hpp"
#include "tdh_socket_config.hpp"
#include "tdh_socket_statistic.hpp"
#include "tdh_socket_encode_response.hpp"
#include "tdh_socket_error.hpp"
#include "tdh_socket_optimize.hpp"
#include "tdh_socket_table_balance.hpp"
#include "thread_and_lock.hpp"
#include "debug_util.hpp"
#include "util.hpp"

#include <easy_define.h>
#include <easy_inet.h>
#include <easy_pool.h>
#include <easy_baseth_pool.h>

namespace taobao {

#define MAX_STREAM_COUNT_FOR_IGNORE 2

static TDHS_INLINE int redispatch_thread_pool_push(easy_thread_pool_t *tp,
		easy_request_t *r, uint64_t hv);

//看是否由于stream堵塞而需要重新 分配
static TDHS_INLINE uint64_t need_redispatch(easy_thread_pool_t *tp,
		easy_request_t *r, uint64_t hv) {
	easy_request_thread_t *rth;
	// dispatch
	if (hv == 0)
		hv = easy_hash_key((long) (r->ms->c));

	rth = (easy_request_thread_t*) (easy_thread_pool_hash(tp, hv));
	tdhs_dbcontext_i *dbcontext = (tdhs_dbcontext_i*) ((rth->args));
	if (dbcontext != NULL) { //dbcontext == NULL 时为未初始化完成的时候
		unsigned long use_steam_count = dbcontext->get_use_steam_count();
		if (use_steam_count > MAX_STREAM_COUNT_FOR_IGNORE) {
			easy_debug_log("TDHS:get_use_steam_count [%d]", use_steam_count);
			return hv + 1; //有stream 堵塞 分配到下一个线程上
		}
	}
	return hv;
}

int on_server_io_process(easy_request_t *r) {
	if (r->retcode == EASY_AGAIN) {
		tdhs_client_wait_t * cond = (tdhs_client_wait_t *) r->args;
		if (cond && cond->is_inited) {
			//对流作处理
			pthread_mutex_lock(&cond->client_wait.mutex);
			if (cond->is_waiting) {
				easy_debug_log("TDHS:wake up for stream!");
				easy_atomic_inc( &r->ms->pool->ref);
				r->ms->c->pool->ref++;
				cond->client_wait.done_count++;
				pthread_cond_signal(&cond->client_wait.cond);
			}
			pthread_mutex_unlock(&cond->client_wait.mutex);
		}
		return EASY_AGAIN;
	}
	tdhs_packet_t *packet = (tdhs_packet_t*) ((r->ipacket));
	if (!packet) {
		return EASY_ERROR;
	}
	{
		easy_debug_log("TDHS:packet info:length[%d],id[%d]",
				packet->length, packet->seq_id);
	}
	tdh_socket_connection_context *c_context =
			(tdh_socket_connection_context*) ((r->ms->c->user_data));
	tb_assert(c_context!=NULL);

	int ret = c_context->shake_hands_if(packet); //在ioth就进行握手
	if (ret != EASY_OK) {
		return ret;
	}
	//进行具体的decode request
	if (c_context->decode(packet) != EASY_OK) {
		r->opacket = r->ipacket; //复用packet
		return tdhs_response_error_global((tdhs_packet_t*) (r->opacket),
				CLIENT_STATUS_BAD_REQUEST,
				CLIENT_ERROR_CODE_DECODE_REQUEST_FAILED);
	}
	easy_debug_log("TDHS:decode done");
	tdhs_optimize_t type = optimize(packet->req);
	//auth
	if (type == TDHS_WRITE ?
			(!c_context->can_write()) : (!c_context->can_read())) {
		r->opacket = r->ipacket; //复用packet
		return tdhs_response_error_global((tdhs_packet_t*) (r->opacket),
				CLIENT_STATUS_FORBIDDEN, CLIENT_ERROR_CODE_UNAUTHENTICATION);
	}
	if (type == TDHS_QUICK) {
		easy_thread_pool_push(request_server_tp, r,
				need_redispatch(request_server_tp, r,
						table_need_balance(packet->req, type,
								packet->reserved)));
	} else if (type == TDHS_SLOW) {
		//throttle
		if (thread_strategy == TDHS_THREAD_LV_3 && tdhs_optimize_on
				&& slow_read_limit()) {
			r->opacket = r->ipacket; //复用packet
			return tdhs_response_error_global((tdhs_packet_t*) (r->opacket),
					CLIENT_STATUS_SERVICE_UNAVAILABLE,
					CLIENT_ERROR_CODE_THROTTLED);
		}
		easy_thread_pool_push(slow_read_request_server_tp, r,
				need_redispatch(slow_read_request_server_tp, r,
						table_need_balance(packet->req, type,
								packet->reserved)));

	} else if (type == TDHS_WRITE) {
		easy_thread_pool_push(write_request_server_tp, r,
				table_need_balance(
						(packet->next) ? (packet->next->req) : packet->req, //如果时batch的话取第二个真正请求进行balance
						type, packet->reserved));
	}
	return EASY_AGAIN;
}

static TDHS_INLINE int redispatch_thread_pool_push(easy_thread_pool_t *tp,
		easy_request_t *r, uint64_t hv) {
	easy_request_thread_t *rth;

	// dispatch
	if (hv == 0)
		hv = easy_hash_key((long) r->ms->c);

	rth = (easy_request_thread_t *) easy_thread_pool_hash(tp, hv);
	easy_list_del(&r->request_list_node);

	// 引用次数
	r->retcode = EASY_AGAIN;
//    r->ms->c->pool->ref ++;
//    easy_atomic_inc(&r->ms->pool->ref);
	easy_pool_set_lock(r->ms->pool);

	easy_spin_lock(&rth->thread_lock);
	easy_list_add_tail(&r->request_list_node, &rth->task_list);
	rth->task_list_count++;
	ev_async_send(rth->loop, &rth->thread_watcher);
	easy_spin_unlock(&rth->thread_lock);

	return EASY_OK;
}

int on_server_process(easy_request_t *r, void *args) {
	easy_debug_log("TDHS:on_server_process");
	r->retcode = EASY_AGAIN; //重置retcode
	tdhs_packet_t *packet = (tdhs_packet_t*) ((r->ipacket));
	tdhs_packet_t **response = (tdhs_packet_t**) ((&(r->opacket)));
	if (!packet) {
		return EASY_ERROR;
	}
	*response = packet; //复用packet

	tdh_socket_connection_context *c_context =
			(tdh_socket_connection_context*) ((r->ms->c->user_data));
	tb_assert(c_context!=NULL);
	tdhs_dbcontext_i *dbcontext = (tdhs_dbcontext_i*) (args);
	tb_assert(dbcontext!=NULL);
	tb_assert(packet->req.status == TDHS_DECODE_DONE);
	if (c_context->get_timeout() > 0) {
		ullong timeout = c_context->get_timeout() * 1000;
		ullong start_time = packet->start_time;
		ullong now = tdhs_micro_time_and_time(dbcontext->get_thd_time());
		easy_debug_log("TDHS:timeout [%llu]us, dealy time [%llu]us",
				timeout, (now - start_time));
		if ((now - start_time) > timeout) {
			return tdhs_response_error_global(*response,
					CLIENT_STATUS_REQUEST_TIME_OUT,
					CLIENT_ERROR_CODE_REQUEST_TIME_OUT);
		}
	}

#ifdef TDHS_ROW_CACHE
	//进行判断
	if (dbcontext->get_type() == TDHS_QUICK
			&& thread_strategy == TDHS_THREAD_LV_3 && tdhs_optimize_on) {
		if (!dbcontext->is_in_cache(packet->req)) {
			optimize_lv3_assign_to_slow_count++;
			//throttle
			if (slow_read_limit()) {
				return tdhs_response_error_global(*response,
						CLIENT_STATUS_SERVICE_UNAVAILABLE,
						CLIENT_ERROR_CODE_THROTTLED);
			}

			// redispatch to SLOW
			redispatch_thread_pool_push(slow_read_request_server_tp, r,
					need_redispatch(slow_read_request_server_tp, r,
							table_need_balance(packet->req, TDHS_SLOW,
									packet->reserved)));
			return EASY_ASYNC;
		}
		optimize_lv3_assign_to_quick_count++;
	}
#endif
	return dbcontext->execute(r);
}

int on_server_connect(easy_connection_t *c) {
	char buffer[32];
	easy_info_log("TDHS:connect: %s connect.\n",
			easy_inet_addr_to_str(&(c->addr), buffer, 32));
	tdh_socket_connection_context *c_context =
			(tdh_socket_connection_context*) (easy_pool_calloc(c->pool,
					sizeof(tdh_socket_connection_context)));
	if (c_context == NULL) {
		easy_error_log("TDHS:easy_pool_alloc failed in tdh_socket_worker");
		return EASY_ERROR;
	}
	if (c_context->init(c) != 0) {
		easy_error_log("TDHS:tdh_socket_worker init failed!");
		return EASY_ERROR;
	}
	c->user_data = c_context;
	return EASY_OK;
}

int on_server_disconnect(easy_connection_t *c) {
	char buffer[32];
	tdh_socket_connection_context *c_context =
			(tdh_socket_connection_context*) (c->user_data);
	easy_info_log("TDHS:connect: %s disconnect.\n",
			easy_inet_addr_to_str(&(c->addr), buffer, 32));
	c_context->destory();
	return EASY_OK;
}

static TDHS_INLINE void on_server_thread_start(void *args, const int type,
		const void* stack_buttom) {
	easy_debug_log("TDHS:on_server_thread_start,type [%d]", type);
	easy_request_thread_t *rth = (easy_request_thread_t *) args;
	tb_assert(rth!=NULL);

	tdhs_dbcontext_i* dbcontext = tdhs_dbcontext_i::create();
	if (dbcontext == NULL) {
		fatal_abort("tdhs_dbcontext_i::create failed!");
	}
	if (dbcontext->init((tdhs_optimize_t) type, stack_buttom) != EASY_OK) {
		fatal_abort("tdhs_dbcontext_i::init failed!");
	}
	rth->args = dbcontext;
	easy_debug_log("TDHS:on_server_thread_start done");
}

static TDHS_INLINE void on_server_thread_destory(void *args) {
	easy_debug_log("TDHS:on_server_thread_destory");
	easy_request_thread_t *rth = (easy_request_thread_t *) args;
	tb_assert(rth!=NULL);
	tdhs_dbcontext_i* dbcontext = (tdhs_dbcontext_i*) rth->args;
	tb_assert(dbcontext!=NULL);
	if (dbcontext->destory() != EASY_OK) {
		fatal_abort("tdhs_dbcontext_i::destory failed!");
	}
	delete dbcontext;
	rth->args = NULL;
	easy_debug_log("TDHS:on_server_thread_destory done");
}

void *request_thread_start(void* args) {
	long stack_used;
	on_server_thread_start(args, TDHS_QUICK, &stack_used);
	void* ret = easy_baseth_on_start(args);
	on_server_thread_destory(args);
	return ret;

}

void *slow_request_thread_start(void* args) {
	long stack_used;
	on_server_thread_start(args, TDHS_SLOW, &stack_used);
	void* ret = easy_baseth_on_start(args);
	on_server_thread_destory(args);
	return ret;
}

void *write_request_thread_start(void* args) {
	long stack_used;
	on_server_thread_start(args, TDHS_WRITE, &stack_used);
	void* ret = easy_baseth_on_start(args);
	on_server_thread_destory(args);
	return ret;
}

int cleanup_request(easy_request_t *r, void *apacket) {
	easy_debug_log("TDHS:cleanup_request!");
	if (r) {
		tdhs_client_wait_t * cond = (tdhs_client_wait_t *) r->args;
		if (cond && cond->is_inited) {
			//对流作处理
			easy_debug_log("TDHS:clean up for stream!");

			pthread_mutex_lock(&cond->client_wait.mutex);
			if (cond->is_waiting) {
				pthread_cond_signal(&cond->client_wait.cond);
			}
			cond->is_closed = true;
			pthread_mutex_unlock(&cond->client_wait.mutex);
			r->args = NULL; //不再需要client_wait了,避免重试时并发导致的heap损坏
		}
	}
	return EASY_OK;
}

}
