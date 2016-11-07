AM_CPPFLAGS = -I/usr/include/sai -I$(top_srcdir)/common

bin_PROGRAMS = asicdump

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

asicdump_SOURCES = asicdump.cpp
asicdump_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
asicdump_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -L$(top_srcdir)/lib/src/.libs -lsairedis
