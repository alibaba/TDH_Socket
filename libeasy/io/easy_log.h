/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#ifndef EASY_LOG_H_
#define EASY_LOG_H_

/**
 * 简单的log输出
 */
#include <easy_define.h>
#include <easy_baseth_pool.h>

EASY_CPP_START

typedef void (*easy_log_print_pt)(const char *message);
typedef enum {
    EASY_LOG_OFF = 1,
    EASY_LOG_FATAL,
    EASY_LOG_ERROR,
    EASY_LOG_WARN,
    EASY_LOG_INFO,
    EASY_LOG_DEBUG,
    EASY_LOG_TRACE,
    EASY_LOG_ALL
} easy_log_level_t;

#define easy_fatal_log(format, args...) if(easy_log_level>=EASY_LOG_FATAL)      \
        easy_log_common(__FILE__, __LINE__, format, ## args);
#define easy_error_log(format, args...) if(easy_log_level>=EASY_LOG_ERROR)      \
        easy_log_common(__FILE__, __LINE__, format, ## args);
#define easy_warn_log(format, args...) if(easy_log_level>=EASY_LOG_WARN)        \
        easy_log_common(__FILE__, __LINE__, format, ## args);
#define easy_info_log(format, args...) if(easy_log_level>=EASY_LOG_INFO)        \
        easy_log_common(__FILE__, __LINE__, format, ## args);
#define easy_debug_log(format, args...) if(easy_log_level>=EASY_LOG_DEBUG)      \
        easy_log_common(__FILE__, __LINE__, format, ## args);
#define easy_trace_log(format, args...) if(easy_log_level>=EASY_LOG_TRACE)      \
        easy_log_common(__FILE__, __LINE__, format, ## args);

// 打印backtrace
#define EASY_PRINT_BT(format, args...)                                                        \
    {char _buffer_stack_[256];{void *array[10];int i, idx=0, n = backtrace(array, 10);        \
            for (i = 0; i < n; i++) idx += snprintf(idx+_buffer_stack_, 25, "%p ", array[i]);}\
        easy_log_common(__FILE__, __LINE__, "%s" format, _buffer_stack_, ## args);}

extern easy_log_level_t easy_log_level;
extern void easy_log_set_print(easy_log_print_pt p);
extern void easy_log_common(const char *file, int line, const char *fmt, ...);
extern void easy_log_print_default(const char *message);

EASY_CPP_END

#endif
