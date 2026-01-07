AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/syncd -I$(top_srcdir)/lib -I$(top_srcdir)/vslib

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

bin_PROGRAMS = tests

tests_SOURCES = \
    main.cpp TestSyncdBrcm.cpp TestSyncdMlnx.cpp TestSyncdNvdaBf.cpp TestSyncdLib.cpp TestDisabledRedisClient.cpp
tests_CXXFLAGS = \
    $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = \
    $(top_srcdir)/syncd/libSyncd.a $(top_srcdir)/lib/libSaiRedis.a $(top_srcdir)/syncd/libSyncdRequestShutdown.a \
    -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -L$(top_srcdir)/vslib/.libs -lsaivs \
    -lhiredis -lswsscommon -lpthread -lzmq $(LDADD_GTEST) $(CODE_COVERAGE_LIBS) $(VPP_LIBS)

TESTS = tests
