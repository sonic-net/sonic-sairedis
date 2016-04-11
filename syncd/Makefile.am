AM_CPPFLAGS =
AM_CPPFLAGS += -I$(top_srcdir)/common

bin_PROGRAMS = syncd

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
		../common/redisclient.cpp \
		../common/saiserialize.cpp \
		../common/saiattribute.cpp \
		../common/saiattributelist.cpp

syncd_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) \
				 -I$(top_srcdir)/lib/inc \
				 -I/usr/include/sai $(SAIFLAGS)
syncd_LDADD = -lhiredis -lswsscommon -lsai -lpthread

