AM_CXXFLAGS = -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

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


libSaiPlayer_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiPlayer_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -std=c++14 $(CODE_COVERAGE_CXXFLAGS)

saiplayer_SOURCES = saiplayer_main.cpp
saiplayer_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saiplayer_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -std=c++14 $(CODE_COVERAGE_CXXFLAGS)
saiplayer_LDADD = libSaiPlayer.a ../syncd/libSyncd.a ../lib/src/libSaiRedis.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)
