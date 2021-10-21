AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/lib

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = \
				main.cpp \
				../../meta/DummySaiInterface.cpp \
				../../meta/MetaTestSaiInterface.cpp \
				../../lib/VirtualObjectIdManager.cpp \
				../../lib/SwitchConfig.cpp \
				../../lib/SwitchConfigContainer.cpp \
				../../lib/ZeroMQChannel.cpp \
				../../lib/Channel.cpp \
				MockMeta.cpp \
				TestAttrKeyMap.cpp \
				TestGlobals.cpp \
				TestMetaKeyHasher.cpp \
				TestNotificationFactory.cpp \
				TestNotificationFdbEvent.cpp \
				TestNotificationPortStateChange.cpp \
				TestNotificationQueuePfcDeadlock.cpp \
				TestNotificationSwitchShutdownRequest.cpp \
				TestNotificationSwitchStateChange.cpp \
				TestNotificationBfdSessionStateChange.cpp \
				TestOidRefCounter.cpp \
				TestPerformanceIntervalTimer.cpp \
				TestPortRelatedSet.cpp \
				TestSaiAttrWrapper.cpp \
				TestSaiAttributeList.cpp \
				TestSaiObject.cpp \
				TestSaiObjectCollection.cpp \
				TestSaiInterface.cpp \
				TestSaiSerialize.cpp \
				TestLegacy.cpp \
				TestLegacyFdbEntry.cpp \
				TestLegacyNeighborEntry.cpp \
				TestLegacyVlan.cpp \
				TestLegacyRouteEntry.cpp \
				TestLegacyOther.cpp \
				TestZeroMQSelectableChannel.cpp \
				TestMeta.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = $(LDADD_GTEST) -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests
