AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/lib -I$(top_srcdir)/syncd

bin_PROGRAMS = saiasiccmp

noinst_LIBRARIES = libAsicCmp.a

libAsicCmp_a_SOURCES = \
				AsicCmp.cpp \
				CommandLineOptions.cpp \
				CommandLineOptionsParser.cpp \
				SaiSwitchAsic.cpp \
				View.cpp \
				ViewCmp.cpp

libAsicCmp_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libAsicCmp_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

saiasiccmp_SOURCES = main.cpp
saiasiccmp_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saiasiccmp_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saiasiccmp_LDADD = libAsicCmp.a \
				   -ldl -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq \
				   $(top_srcdir)/syncd/libSyncd.a $(top_srcdir)/lib/libSaiRedis.a -lhiredis -lswsscommon -lz $(CODE_COVERAGE_LIBS)

TESTS = test.sh
