AM_CPPFLAGS = -I$(top_srcdir)/lib/inc -I/usr/include/sai

bin_PROGRAMS = saiplayer

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

saiplayer_SOURCES = saiplayer.cpp
saiplayer_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
saiplayer_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -L$(top_srcdir)/lib/src/.libs -lsairedis
