/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * thread_and_lock.hpp
 *
 *  Created on: 2011-8-25
 *      Author: wentong
 */

#ifndef THREAD_AND_LOCK_HPP_
#define THREAD_AND_LOCK_HPP_

#include "debug_util.hpp"
#include <easy_atomic.h>

#ifdef __linux__
#include <pthread.h>
#else
//XXX  Compatibility with other operating system
#endif

#define pthread_wait_cond(mutex, cond) \
	pthread_mutex_lock((mutex)); \
	pthread_cond_wait((cond), (mutex)); \
	pthread_mutex_unlock((mutex));

class pthread_mutex_locker {
public:
	pthread_mutex_locker(pthread_mutex_t *_lock) :
			lock(_lock) {
		tb_assert(lock!=NULL)
		pthread_mutex_lock(lock);
	}
	~pthread_mutex_locker() {
		pthread_mutex_unlock(lock);
	}
private:
	pthread_mutex_t *lock;
};

class easy_spin_locker {
public:
	easy_spin_locker(easy_atomic_t *_lock) :
			lock(_lock) {
		tb_assert(lock!=NULL)
		easy_spin_lock(lock);
	}
	~easy_spin_locker() {
		easy_spin_unlock(lock);
	}
private:
	easy_atomic_t *lock;
};

#endif /* THREAD_AND_LOCK_HPP_ */
