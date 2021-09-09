AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/syncd -I$(top_srcdir)/lib -I$(top_srcdir)/vslib

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = main.cpp \
				TestCommandLineOptions.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/syncd/libSyncd.a -lswsscommon -lpthread -L$(top_srcdir)/lib/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)
