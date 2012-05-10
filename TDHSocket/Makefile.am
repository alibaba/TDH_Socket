pkgplugindir =  $(PLUGIN_DIR)
pkgplugin_LTLIBRARIES = tdhsocket.la
noinst_HEADERS = mysql_inc.hpp tdh_socket_config.hpp tdh_socket_handler.hpp tdh_socket_connection_context.hpp \
					tdh_socket_dbcontext.hpp tdh_socket_share.hpp \
					tdh_socket_encode_response.hpp tdh_socket_dbutil.hpp \
					tdh_socket_statistic.hpp tdh_socket_monitor.hpp tdh_socket_optimize.hpp \
					tdh_socket_decode_request_binary.hpp tdh_socket_request_thread.hpp \
					tdh_socket_table_balance.hpp tdh_socket_thd.hpp default_mysql_sysvar_for_tdh_socket.hpp
tdhsocket_la_LDFLAGS = -module ../libtdhs/libtdhs.la
tdhsocket_la_CFLAGS = $(AM_CFLAGS) $(MYSQL_INC) $(MYSQL_CFLAGS)  -I../libtdhs $(EASY_INCLUDES)
tdhsocket_la_CXXFLAGS = ${tdhsocket_la_CFLAGS}
tdhsocket_la_SOURCES = tdh_socket.cpp tdh_socket_config.cpp tdh_socket_handler.cpp  \
						tdh_socket_connection_context.cpp \
						tdh_socket_dbcontext.cpp tdh_socket_share.cpp \
						tdh_socket_statistic.cpp \
						tdh_socket_monitor.cpp tdh_socket_optimize.cpp \
						tdh_socket_decode_request_binary.cpp tdh_socket_request_thread.cpp \
						tdh_socket_table_balance.cpp

am__DEPENDENCIES_1=../libtdhs/.libs/libtdhs.a
${PRESET_LDADD}:
	rm -f ${top_srcdir}/libtdhs/libtdhs.la
	cd ${top_srcdir}/libtdhs && make