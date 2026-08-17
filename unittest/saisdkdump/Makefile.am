AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/saisdkdump

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = main.cpp TestProfileMap.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/saisdkdump/libsaisdkdump_profile.a \
              -lswsscommon -lpthread $(CODE_COVERAGE_LIBS)

TESTS = tests
