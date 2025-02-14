AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/syncd -I$(top_srcdir)/lib -I$(top_srcdir)/vslib -I$(top_srcdir)/meta

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main -lgmock

tests_SOURCES = main.cpp \
                MockableSaiInterface.cpp \
                MockHelper.cpp \
				MockableSaiSwitchInterface.cpp \
				TestBestCandidateFinder.cpp \
				TestCommandLineOptions.cpp \
				TestConcurrentQueue.cpp \
				TestFlexCounter.cpp \
				TestVirtualOidTranslator.cpp \
				TestNotificationQueue.cpp \
				TestNotificationProcessor.cpp \
				TestNotificationHandler.cpp \
				TestMdioIpcServer.cpp \
				TestPortStateChangeHandler.cpp \
				TestWorkaround.cpp \
				TestSyncd.cpp \
				TestVendorSai.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDFLAGS = -Wl,-rpath,$(top_srcdir)/lib/.libs -Wl,-rpath,$(top_srcdir)/meta/.libs
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/syncd/libSyncdRequestShutdown.a $(top_srcdir)/syncd/libSyncd.a $(top_srcdir)/vslib/libSaiVS.a $(top_srcdir)/syncd/libMdioIpcClient.a \
			  -lhiredis -lswsscommon -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 -lpthread -L$(top_srcdir)/lib/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests
