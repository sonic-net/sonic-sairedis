AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/saiconfig -I$(top_srcdir)/lib -I$(top_srcdir)/meta

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main -lgmock

tests_SOURCES = main.cpp \
				TestSaiConfig.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -fno-access-control
tests_LDFLAGS = -Wl,-rpath,$(top_srcdir)/lib/.libs -Wl,-rpath,$(top_srcdir)/meta/.libs
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/saiconfig/libsaiconfig.a \
			  -L$(top_srcdir)/lib/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
			  -lhiredis -lswsscommon -lpthread -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests
