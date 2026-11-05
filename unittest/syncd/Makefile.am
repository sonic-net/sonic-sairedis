AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/syncd -I$(top_srcdir)/lib -I$(top_srcdir)/vslib -I$(top_srcdir)/meta

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main -lgmock

# Stub MDIO access libraries dlopen()ed by TestGearboxMdioAccess.cpp.
noinst_LTLIBRARIES = libMdioTest.la libMdioTestMissingSymbols.la

libMdioTest_la_SOURCES = MdioTestLib.c
libMdioTest_la_CPPFLAGS = $(SAIINC) -DMDIO_TEST_LIB_MARKER="\"$(abs_builddir)/mdiotestlib.loaded\""
libMdioTestMissingSymbols_la_SOURCES = MdioTestLibMissingSymbols.c

tests_SOURCES = main.cpp \
                MockableSaiInterface.cpp \
                MockHelper.cpp \
				MockableSaiSwitchInterface.cpp \
				TestBestCandidateFinder.cpp \
				TestMySidWarmboot.cpp \
				TestAttrVersionChecker.cpp \
				TestCommandLineOptions.cpp \
				TestConcurrentQueue.cpp \
				TestFlexCounter.cpp \
				TestPortPhyAttr.cpp \
				TestTimerWatchdog.cpp \
				TestPortPhySerdesAttr.cpp \
				TestVirtualOidTranslator.cpp \
				TestNotificationQueue.cpp \
				TestNotificationProcessor.cpp \
				TestNotificationHandler.cpp \
				TestMdioIpcServer.cpp \
				TestPortStateChangeHandler.cpp \
				TestSelectablesTracker.cpp \
				TestWorkaround.cpp \
				TestSyncd.cpp \
				TestVendorSai.cpp \
				TestFlowDump.cpp \
				TestAsan.cpp \
				TestGearboxMdioAccess.cpp \
				$(top_srcdir)/syncd/Asan.cpp

MDIO_TEST_LIB_DIR = $(abs_builddir)/.libs
MDIO_TEST_LIB_MARKER_PATH = $(abs_builddir)/mdiotestlib.loaded

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -fno-access-control \
				 -DMDIO_TEST_LIB_PATH="\"$(MDIO_TEST_LIB_DIR)/libMdioTest.so\"" \
				 -DMDIO_TEST_LIB_MISSING_SYMBOLS_PATH="\"$(MDIO_TEST_LIB_DIR)/libMdioTestMissingSymbols.so\"" \
				 -DMDIO_TEST_LIB_MARKER="\"$(MDIO_TEST_LIB_MARKER_PATH)\""

tests_DEPENDENCIES = libMdioTest.la libMdioTestMissingSymbols.la
tests_LDFLAGS = -Wl,-rpath,$(top_srcdir)/lib/.libs -Wl,-rpath,$(top_srcdir)/meta/.libs
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/syncd/libSyncdRequestShutdown.a $(top_srcdir)/syncd/libSyncd.a $(top_srcdir)/vslib/libSaiVS.a $(top_srcdir)/syncd/libMdioIpcClient.a \
			  -lhiredis -lswsscommon -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 -lpthread -L$(top_srcdir)/lib/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq -lz $(CODE_COVERAGE_LIBS) $(VPP_LIBS)

TESTS = tests
