AM_CXXFLAGS = -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/experimental -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/lib/inc -I$(top_srcdir)/meta -I$(top_srcdir)/syncd

bin_PROGRAMS = saiasiccmp

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

noinst_LIBRARIES = libAsicCmp.a

libAsicCmp_a_SOURCES = \
				AsicCmp.cpp \
				View.cpp \
				ViewCmp.cpp \
				SaiSwitchAsic.cpp \
				CommandLineOptions.cpp \
				CommandLineOptionsParser.cpp

libAsicCmp_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libAsicCmp_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

saiasiccmp_SOURCES = main.cpp
saiasiccmp_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saiasiccmp_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saiasiccmp_LDADD = libAsicCmp.a \
				   -lsaimetadata -lsaimeta -ldl -lhiredis -lswsscommon -lpthread -lzmq \
				   $(top_srcdir)/syncd/libSyncd.a \
				   -L$(top_srcdir)/syncd/.libs \
				   $(top_srcdir)/lib/src/libSaiRedis.a \
				   -L$(top_srcdir)/meta/.libs \
				   $(CODE_COVERAGE_LIBS)

TESTS = test.sh
