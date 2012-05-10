LIBDIRS = memory util thread io packet
SUBDIRS = include $(LIBDIRS) 

noinst_LTLIBRARIES=libeasy.la
libeasy_la_SOURCES=
libeasy_la_LIBADD=$(foreach sub,$(LIBDIRS),$(sub)/libeasy_$(sub).la)

