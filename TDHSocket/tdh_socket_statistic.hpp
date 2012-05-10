/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_statistic.hpp
 *
 *  Created on: 2011-10-26
 *      Author: wentong
 */

#ifndef TDH_SOCKET_STATISTIC_HPP_
#define TDH_SOCKET_STATISTIC_HPP_

namespace taobao {

extern unsigned long long int open_tables_count;

extern unsigned long long int lock_tables_count;

extern unsigned long long int unlock_tables_count;

extern unsigned long long int close_tables_count;

extern unsigned long long int get_count;

extern unsigned long long int count_count;

extern unsigned long long int update_count;

extern unsigned long long int delete_count;

extern unsigned long long int insert_count;

extern unsigned long long int batch_count;

extern long long int last_io_read_per_second;

extern unsigned long long int optimize_lv3_assign_to_quick_count;

extern unsigned long long int optimize_lv3_assign_to_slow_count;

extern unsigned long long int throttle_count;

} // namespace taobao

#endif /* TDH_SOCKET_STATISTIC_HPP_ */
