AM_CFLAGS=-I$(top_srcdir)/src/include -I$(top_srcdir)/src/util \
    -I$(top_srcdir)/src/thread -I$(top_srcdir)/src/memory $(EASY_INCLUDES)

EXTRA_DIST = ev_vars.h ev_wrap.h ev_epoll.c 
noinst_HEADERS=easy_connection.h       \
	easy_io.h                       \
	easy_file.h                     \
	easy_message.h                  \
	easy_request.h                  \
	easy_socket.h                   \
	easy_client.h                   \
	easy_io_struct.h                \
	easy_baseth_pool.h              \
	easy_log.h                      \
	ev.h

noinst_LTLIBRARIES=libeasy_io.la
libeasy_io_la_SOURCES = easy_socket.c   \
	easy_io.c                       \
	easy_file.c                     \
	ev.c                            \
	easy_connection.c               \
	easy_message.c                  \
	easy_request.c                  \
	easy_baseth_pool.c              \
	easy_log.c                      \
	easy_client.c
