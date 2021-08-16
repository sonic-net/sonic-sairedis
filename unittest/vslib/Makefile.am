AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/vslib

bin_PROGRAMS = tests testslibsaivs

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = main.cpp \
				TestBuffer.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/vslib/libSaiVS.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

testslibsaivs_SOURCES =	main_libsaivs.cpp \
				test_sai_vs_acl.cpp \
				test_sai_vs_bfd.cpp \
				test_sai_vs_bmtor.cpp \
				test_sai_vs_bridge.cpp \
				test_sai_vs_buffer.cpp \
				test_sai_vs_counter.cpp \
				test_sai_vs_debug_counter.cpp \
				test_sai_vs_dtel.cpp \
				test_sai_vs_fdb.cpp \
				test_sai_vs_ipmc.cpp \
				test_sai_vs_l2mc.cpp \
				test_sai_vs_l2mcgroup.cpp \
				test_sai_vs_lag.cpp \
				test_sai_vs_ipmc_group.cpp \
				test_sai_vs_isolation_group.cpp \
				test_sai_vs_hash.cpp \
				test_sai_vs_vlan.cpp \
				test_sai_vs_hostif.cpp \
				test_sai_vs_wred.cpp

testslibsaivs_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
testslibsaivs_LDADD = $(LDADD_GTEST) -L$(top_srcdir)/vslib/.libs -lsaivs -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests testslibsaivs
