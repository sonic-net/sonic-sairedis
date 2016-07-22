AM_CPPFLAGS =
AM_CPPFLAGS += -I$(top_srcdir)/common

bin_PROGRAMS = syncd syncd_request_shutdown

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

if SAIBCM
SAIFLAGS = -DBRCMSAI
endif

if SAIMLNX
SAIFLAGS = -DMLNXSAI
endif

syncd_SOURCES = syncd.cpp \
		syncd_init.cpp \
		syncd_reinit.cpp \
		syncd_hard_reinit.cpp \
		syncd_notifications.cpp \
		syncd_counters.cpp \
		../common/redisclient.cpp \
		../common/saiserialize.cpp \
		../common/saiattribute.cpp \
		../common/saiattributelist.cpp

syncd_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) \
				 -I$(top_srcdir)/lib/inc \
				 -I/usr/include/sai $(SAIFLAGS)
syncd_LDADD = -lhiredis -lswsscommon -lsai -lpthread

if SAITHRIFT
SAI_RPC_INCLUDE_DIR = ../libsaiserver/include
SAI_RPC_LIB_DIR = ../libsaiserver/lib
syncd_CPPFLAGS += -I$(SAI_RPC_INCLUDE_DIR) -DSAITHRIFT=yes
syncd_LDADD += -L$(SAI_RPC_LIB_DIR) -lrpcserver
endif

syncd_request_shutdown_SOURCES = syncd_request_shutdown.cpp

syncd_request_shutdown_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)

syncd_request_shutdown_LDADD = -lhiredis -lswsscommon -lpthread

