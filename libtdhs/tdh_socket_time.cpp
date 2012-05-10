/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/* Copyright (c) 2004, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* get time since epoc in 100 nanosec units */
/* thus to get the current time we should use the system function
   with the highest possible resolution */

/*
   TODO: in functions my_micro_time() and my_micro_time_and_time() there
   exists some common code that should be merged into a function.
*/
#include "tdh_socket_time.hpp"
#include <sys/time.h>
#include <stddef.h>

namespace taobao {

ullong tdhs_micro_time() {
#if defined(__WIN__)
	ullong newtime;
	GetSystemTimeAsFileTime((FILETIME*)&newtime);
	return (newtime/10);
#elif defined(HAVE_GETHRTIME)
	return gethrtime()/1000;
#else
	ullong newtime;
	struct timeval t;
	/*
	 The following loop is here because gettimeofday may fail on some systems
	 */
	while (gettimeofday(&t, NULL) != 0) {
	}
	newtime = (ullong) t.tv_sec * 1000000 + t.tv_usec;
	return newtime;
#endif  /* defined(__WIN__) */
}

/* Difference between GetSystemTimeAsFileTime() and now() */
#define OFFSET_TO_EPOCH 116444736000000000ULL

ullong tdhs_micro_time_and_time(time_t *time_arg)
{
#ifdef _WIN32
  ullong newtime;
  GetSystemTimeAsFileTime((FILETIME*)&newtime);
  *time_arg= (time_t) ((newtime - OFFSET_TO_EPOCH) / 10000000);
  return (newtime/10);
#else
  ullong newtime;
  struct timeval t;
  /*
    The following loop is here because gettimeofday may fail on some systems
  */
  while (gettimeofday(&t, NULL) != 0)
  {}
  *time_arg= t.tv_sec;
  newtime= (ullong)t.tv_sec * 1000000 + t.tv_usec;
  return newtime;
#endif
}

} // namespace taobao

