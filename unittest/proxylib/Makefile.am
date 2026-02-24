AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/proxylib -I$(top_srcdir)/lib

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = \
				main.cpp \
				TestProxy.cpp \
				TestSai.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/proxylib/libSaiProxy.a \
			  $(top_srcdir)/lib/libSaiRedis.a \
			  $(top_srcdir)/syncd/libSyncd.a \
			  -lhiredis -lswsscommon -lpthread \
			  -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
			  -lzmq -lz $(CODE_COVERAGE_LIBS)

TESTS = tests
