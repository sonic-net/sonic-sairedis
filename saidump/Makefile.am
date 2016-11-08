AM_CPPFLAGS = -I/usr/include/sai -I$(top_srcdir)/common

bin_PROGRAMS = saidump

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

saidump_SOURCES = saidump.cpp
saidump_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
saidump_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -L$(top_srcdir)/lib/src/.libs -lsairedis
