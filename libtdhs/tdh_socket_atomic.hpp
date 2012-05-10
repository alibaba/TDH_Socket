/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_atomic.hpp
 *
 *  Created on: 2011-8-19
 *      Author: wentong
 */

#ifndef TDH_SOCKET_ATOMIC_HPP_
#define TDH_SOCKET_ATOMIC_HPP_

#if defined(HAVE_IB_GCC_ATOMIC_BUILTINS)

#define HAVE_ATOMIC_BUILTINS

/**********************************************************//**
 Returns true if swapped, ptr is pointer to target, old_val is value to
 compare to, new_val is the value to swap in. */

# define os_compare_and_swap(ptr, old_val, new_val) \
	__sync_bool_compare_and_swap(ptr, old_val, new_val)

# define os_compare_and_swap_ulint(ptr, old_val, new_val) \
	os_compare_and_swap(ptr, old_val, new_val)

# define os_compare_and_swap_lint(ptr, old_val, new_val) \
	os_compare_and_swap(ptr, old_val, new_val)

# ifdef HAVE_IB_ATOMIC_PTHREAD_T_GCC
#  define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
	os_compare_and_swap(ptr, old_val, new_val)
#  define INNODB_RW_LOCKS_USE_ATOMICS
#  define IB_ATOMICS_STARTUP_MSG \
	"Mutexes and rw_locks use GCC atomic builtins"
# else /* HAVE_IB_ATOMIC_PTHREAD_T_GCC */
#  define IB_ATOMICS_STARTUP_MSG \
	"Mutexes use GCC atomic builtins, rw_locks do not"
# endif /* HAVE_IB_ATOMIC_PTHREAD_T_GCC */

/**********************************************************//**
 Returns the resulting value, ptr is pointer to target, amount is the
 amount of increment. */

# define os_atomic_increment(ptr, amount) \
	__sync_add_and_fetch(ptr, amount)

# define os_atomic_increment_lint(ptr, amount) \
	os_atomic_increment(ptr, amount)

# define os_atomic_increment_ulint(ptr, amount) \
	os_atomic_increment(ptr, amount)

/**********************************************************//**
 Returns the old value of *ptr, atomically sets *ptr to new_val */

# define os_atomic_test_and_set_byte(ptr, new_val) \
	__sync_lock_test_and_set(ptr, new_val)

#elif defined(HAVE_IB_SOLARIS_ATOMICS)

#define HAVE_ATOMIC_BUILTINS

/* If not compiling with GCC or GCC doesn't support the atomic
 intrinsics and running on Solaris >= 10 use Solaris atomics */

#include <atomic.h>

/**********************************************************//**
 Returns true if swapped, ptr is pointer to target, old_val is value to
 compare to, new_val is the value to swap in. */

# define os_compare_and_swap_ulint(ptr, old_val, new_val) \
	(atomic_cas_ulong(ptr, old_val, new_val) == old_val)

# define os_compare_and_swap_lint(ptr, old_val, new_val) \
	((lint)atomic_cas_ulong((ulong_t*) ptr, old_val, new_val) == old_val)

# ifdef HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS
#  if SIZEOF_PTHREAD_T == 4
#   define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
	((pthread_t)atomic_cas_32(ptr, old_val, new_val) == old_val)
#  elif SIZEOF_PTHREAD_T == 8
#   define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
	((pthread_t)atomic_cas_64(ptr, old_val, new_val) == old_val)
#  else
#   error "SIZEOF_PTHREAD_T != 4 or 8"
#  endif /* SIZEOF_PTHREAD_T CHECK */
#  define INNODB_RW_LOCKS_USE_ATOMICS
#  define IB_ATOMICS_STARTUP_MSG \
	"Mutexes and rw_locks use Solaris atomic functions"
# else /* HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS */
#  define IB_ATOMICS_STARTUP_MSG \
	"Mutexes use Solaris atomic functions, rw_locks do not"
# endif /* HAVE_IB_ATOMIC_PTHREAD_T_SOLARIS */

/**********************************************************//**
 Returns the resulting value, ptr is pointer to target, amount is the
 amount of increment. */

# define os_atomic_increment_lint(ptr, amount) \
	atomic_add_long_nv((ulong_t*) ptr, amount)

# define os_atomic_increment_ulint(ptr, amount) \
	atomic_add_long_nv(ptr, amount)

/**********************************************************//**
 Returns the old value of *ptr, atomically sets *ptr to new_val */

# define os_atomic_test_and_set_byte(ptr, new_val) \
	atomic_swap_uchar(ptr, new_val)
#elif defined(HAVE_WINDOWS_ATOMICS)

#define HAVE_ATOMIC_BUILTINS

/* On Windows, use Windows atomics / interlocked */
# ifdef _WIN64
#  define win_cmp_and_xchg InterlockedCompareExchange64
#  define win_xchg_and_add InterlockedExchangeAdd64
# else /* _WIN64 */
#  define win_cmp_and_xchg InterlockedCompareExchange
#  define win_xchg_and_add InterlockedExchangeAdd
# endif

/**********************************************************//**
 Returns true if swapped, ptr is pointer to target, old_val is value to
 compare to, new_val is the value to swap in. */

# define os_compare_and_swap_ulint(ptr, old_val, new_val) \
	(win_cmp_and_xchg(ptr, new_val, old_val) == old_val)

# define os_compare_and_swap_lint(ptr, old_val, new_val) \
	(win_cmp_and_xchg(ptr, new_val, old_val) == old_val)

/* windows thread objects can always be passed to windows atomic functions */
# define os_compare_and_swap_thread_id(ptr, old_val, new_val) \
	(InterlockedCompareExchange(ptr, new_val, old_val) == old_val)
# define INNODB_RW_LOCKS_USE_ATOMICS
# define IB_ATOMICS_STARTUP_MSG \
	"Mutexes and rw_locks use Windows interlocked functions"

/**********************************************************//**
 Returns the resulting value, ptr is pointer to target, amount is the
 amount of increment. */

# define os_atomic_increment_lint(ptr, amount) \
	(win_xchg_and_add(ptr, amount) + amount)

# define os_atomic_increment_ulint(ptr, amount) \
	((ulint) (win_xchg_and_add(ptr, amount) + amount))

/**********************************************************//**
 Returns the old value of *ptr, atomically sets *ptr to new_val.
 InterlockedExchange() operates on LONG, and the LONG will be
 clobbered */

# define os_atomic_test_and_set_byte(ptr, new_val) \
	((byte) InterlockedExchange(ptr, new_val))
#endif

#include "thread_and_lock.hpp"
# define pthread_atomic_increment(ptr, amount , mutex) \
	pthread_mutex_lock((mutex)); \
	*(ptr) = *(ptr) + (amount); \
	pthread_mutex_unlock((mutex));

#endif /* TDH_SOCKET_ATOMIC_HPP_ */
