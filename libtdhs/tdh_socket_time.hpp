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

#ifndef TDH_SOCKET_TIME_HPP_
#define TDH_SOCKET_TIME_HPP_

#include "debug_util.hpp"

namespace taobao {

typedef unsigned long long int ullong;

ullong tdhs_micro_time();

ullong tdhs_micro_time_and_time(time_t *time_arg);

} // namespace taobao

#endif /* TDH_SOCKET_TIME_HPP_ */
