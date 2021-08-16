AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/vslib

bin_PROGRAMS = tests testslibsaivs

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = \
				main.cpp \
				TestBuffer.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/vslib/libSaiVS.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

testslibsaivs_SOURCES = \
				main_libsaivs.cpp \
				test_sai_vs_vlan.cpp \
				test_sai_vs_wred.cpp

testslibsaivs_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
testslibsaivs_LDADD = $(LDADD_GTEST) -L$(top_srcdir)/vslib/.libs -lsaivs -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests testslibsaivs
