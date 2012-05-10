/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * debug_util.hpp
 *
 *  Created on: 2011-8-26
 *      Author: wentong
 */

#ifndef DEBUG_UTIL_HPP_
#define DEBUG_UTIL_HPP_

#include "util.hpp"

namespace taobao {

#define tb_assert(S)
#define TDHS_INLINE inline

#ifdef TDHS_DEBUG

#undef TDHS_INLINE
#define TDHS_INLINE

#undef tb_assert
#define tb_assert(S) \
	if(!(S)){ \
		fatal_abort("tb_assert failed!") \
	}

#endif

} // namespace taobao

#endif /* DEBUG_UTIL_HPP_ */
