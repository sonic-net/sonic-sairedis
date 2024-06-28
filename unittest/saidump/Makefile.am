AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/saidump -I$(top_srcdir)/lib -I$(top_srcdir)/vslib -I$(top_srcdir)/meta

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = main.cpp TestSaiDump.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDFLAGS = -D_UNITTEST_
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/saidump/libsaidump.a  -lhiredis -lswsscommon -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 -lpthread -L$(top_srcdir)/lib/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests