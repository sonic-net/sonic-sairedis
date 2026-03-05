AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib

bin_PROGRAMS = saiplayer

noinst_LIBRARIES = libSaiPlayer.a

libSaiPlayer_a_SOURCES = \
						 CommandLineOptions.cpp \
						 CommandLineOptionsParser.cpp \
						 SaiPlayer.cpp

libSaiPlayer_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiPlayer_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

saiplayer_SOURCES = saiplayer_main.cpp
saiplayer_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saiplayer_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saiplayer_LDADD =  libSaiPlayer.a $(top_srcdir)/syncd/libSyncd.a $(top_srcdir)/lib/libSaiRedis.a \
				   -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq -lz $(CODE_COVERAGE_LIBS)
