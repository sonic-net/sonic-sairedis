AM_CPPFLAGS = -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

bin_PROGRAMS = saidump

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

saidump_SOURCES = saidump.cpp
saidump_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
saidump_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -L$(top_srcdir)/lib/src/.libs -lsairedis
