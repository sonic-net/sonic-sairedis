AM_CPPFLAGS = -I/usr/include/sai -I$(top_srcdir)/lib/inc

bin_PROGRAMS = saiplayer

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

saiplayer_SOURCES = saiplayer.cpp
saiplayer_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
saiplayer_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -L$(top_srcdir)/lib/src/.libs -lsairedis
