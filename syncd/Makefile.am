AM_CPPFLAGS = -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/lib/inc -I/usr/include/sai 

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

if SAICAVM
SAIFLAGS = -DCAVMSAI
endif

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/src/.libs -lsaivs
else
SAILIB=-lsai
endif

syncd_SOURCES = \
				syncd.cpp \
				syncd_cli.cpp \
				syncd_init.cpp \
				syncd_reinit.cpp \
				syncd_hard_reinit.cpp \
				syncd_notifications.cpp \
				syncd_counters.cpp

syncd_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS)
syncd_LDADD = -lhiredis -lswsscommon $(SAILIB) -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata

if SAITHRIFT
SAI_RPC_INCLUDE_DIR = ../libsaiserver/include
SAI_RPC_LIB_DIR = ../libsaiserver/lib
syncd_CPPFLAGS += -I$(SAI_RPC_INCLUDE_DIR) -DSAITHRIFT=yes
syncd_LDADD += -L$(SAI_RPC_LIB_DIR) -lrpcserver -lthrift
endif

syncd_request_shutdown_SOURCES = syncd_request_shutdown.cpp
syncd_request_shutdown_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
syncd_request_shutdown_LDADD = -lhiredis -lswsscommon -lpthread
