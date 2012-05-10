noinst_HEADERS =   util.hpp tdh_socket_atomic.hpp thread_and_lock.hpp \
					tdh_socket_protocol.hpp debug_util.hpp \
					tdh_socket_define.hpp tdh_socket_time.hpp \
					tdh_socket_bloom_filter.h tdh_socket_error.hpp
noinst_LTLIBRARIES = libtdhs.la
libtdhs_la_SOURCES = tdh_socket_protocol.cpp tdh_socket_time.cpp tdh_socket_bloom_filter.c \
                                        tdh_socket_error.cpp
libtdhs_la_LDFLAGS = -static -L../libeasy -module ../libeasy/libeasy.la
libtdhs_la_CFLAGS = $(AM_CFLAGS) $(EASY_INCLUDES)
libtdhs_la_CXXFLAGS = ${libtdhs_la_CFLAGS}

am__DEPENDENCIES_1=../libeasy/.libs/libeasy.a
../libeasy/.libs/libeasy.a:
	rm -f ${top_srcdir}/libeasy/libeasy.la
	cd ${top_srcdir}/libeasy && make
	