AM_CPPFLAGS = -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

bin_PROGRAMS = saiplayer

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

noinst_LIBRARIES = libSaiPlayer.a
libSaiPlayer_a_SOURCES = \
						 CommandLineOptions.cpp \
						 CommandLineOptionsParser.cpp \
						 SaiPlayer.cpp


libSaiPlayer_a_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) -std=c++14

saiplayer_SOURCES = saiplayer.cpp
saiplayer_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) -std=c++14
saiplayer_LDADD = libSaiPlayer.a ../syncd/libSyncd.a ../lib/src/libSaiRedis.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq
