LIBDIRS = memory util thread io packet
#SUBDIRS = include $(LIBDIRS)
SUBDIRS = include memory util thread io packet

noinst_LTLIBRARIES=libeasy.la
libeasy_la_SOURCES=

#libeasy_la_LIBADD= $(foreach sub,$(LIBDIRS),$(sub)/libeasy_$(sub).la)
libeasy_la_LIBADD= memory/libeasy_memory.la \
				   util/libeasy_util.la \
				   thread/libeasy_thread.la \
				   io/libeasy_io.la \
				   packet/libeasy_packet.la 



