AM_CPPFLAGS = -I/usr/include/sai -I$(top_srcdir)/common

bin_PROGRAMS = player

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

player_SOURCES = player.cpp
player_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
player_LDADD = -lhiredis -lswsscommon -lsairedis -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata
