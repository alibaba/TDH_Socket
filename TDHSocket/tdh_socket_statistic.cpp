/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * tdh_socket_statistic.cpp
 *
 *  Created on: 2011-10-26
 *      Author: wentong
 */

#include "tdh_socket_statistic.hpp"

namespace taobao {

unsigned long long int open_tables_count = 0;

unsigned long long int lock_tables_count = 0;

unsigned long long int unlock_tables_count = 0;

unsigned long long int close_tables_count = 0;

unsigned long long int get_count = 0;

unsigned long long int count_count = 0;

unsigned long long int update_count = 0;

unsigned long long int delete_count = 0;

unsigned long long int insert_count = 0;

unsigned long long int batch_count = 0;

long long int last_io_read_per_second = 0;

unsigned long long int optimize_lv3_assign_to_quick_count = 0;

unsigned long long int optimize_lv3_assign_to_slow_count = 0;

unsigned long long int throttle_count = 0;

} // namespace taobao

