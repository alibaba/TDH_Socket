/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


/*
 * mysql_inc.hpp
 *
 *  Created on: 2011-8-12
 *      Author: wentong
 */

#ifndef MYSQL_INC_HPP_
#define MYSQL_INC_HPP_


#ifndef HAVE_CONFIG_H
#define HAVE_CONFIG_H
#endif

#define MYSQL_DYNAMIC_PLUGIN
#define MYSQL_SERVER 1

#include "my_config.h"

#include "mysql_version.h"

#if MYSQL_VERSION_ID >= 50505
#include <my_pthread.h>
#include <sql_priv.h>
#include "sql_class.h"
#include "unireg.h"
#include "lock.h"
#include "key.h" // key_copy()
#include <my_global.h>
#include <mysql/plugin.h>
#include <transaction.h>
#include <sql_base.h>
#include "sql_show.h" //for schema_table

#define safeFree(X) my_free(X)

#define  tdhs_mysql_cond_timedwait  mysql_cond_timedwait
#define  tdhs_mysql_mutex_lock  mysql_mutex_lock
#define  tdhs_mysql_mutex_unlock  mysql_mutex_unlock
#define  tdhs_mysql_cond_broadcast mysql_cond_broadcast

#define current_stmt_binlog_row_based  is_current_stmt_binlog_format_row
#define clear_current_stmt_binlog_row_based  clear_current_stmt_binlog_format_row

#else

#include "mysql_priv.h"
#define  tdhs_mysql_cond_timedwait  pthread_cond_timedwait
#define  tdhs_mysql_mutex_lock  pthread_mutex_lock
#define  tdhs_mysql_mutex_unlock  pthread_mutex_unlock
#define  tdhs_mysql_cond_broadcast pthread_cond_broadcast

#endif

#undef min
#undef max


#endif /* MYSQL_INC_HPP_ */
