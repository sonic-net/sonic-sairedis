AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/lib

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main -lgmock

tests_SOURCES = \
				main.cpp \
				../../meta/NumberOidIndexGenerator.cpp \
				TestSwitch.cpp \
				TestClientConfig.cpp \
				TestClientServerSai.cpp \
				TestContext.cpp \
				TestContextConfig.cpp \
				TestContextConfigContainer.cpp \
				TestUtils.cpp \
				TestVirtualObjectIdManager.cpp \
				TestZeroMQChannel.cpp \
				TestSwitchContainer.cpp \
				TestSwitchConfigContainer.cpp \
				TestSkipRecordAttrContainer.cpp \
				TestServerConfig.cpp \
				TestRedisVidIndexGenerator.cpp \
				TestRecorder.cpp \
				TestRedisChannel.cpp \
				TestClientSai.cpp \
				TestRedisRemoteSaiInterface.cpp \
				TestServerSai.cpp \
				TestSai.cpp \
				MockSaiInterface.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/lib/libSaiRedis.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests
