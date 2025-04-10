AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/vslib -I$(top_srcdir)/lib -I/usr/include/libnl3

bin_PROGRAMS = tests

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = main.cpp \
				TestBuffer.cpp \
				TestContextConfigContainer.cpp \
				TestCorePortIndexMap.cpp \
				TestCorePortIndexMapContainer.cpp \
				TestCorePortIndexMapFileParser.cpp \
				TestEventPayloadNetLinkMsg.cpp \
				TestEventPayloadPacket.cpp \
				TestEventQueue.cpp \
				TestFdbInfo.cpp \
				TestSaiAttrWrap.cpp \
				TestLaneMap.cpp \
				TestLaneMapContainer.cpp \
				TestLaneMapFileParser.cpp \
				TestMACsecAttr.cpp \
				TestMACsecEgressFilter.cpp \
				TestMACsecForwarder.cpp \
				TestMACsecIngressFilter.cpp \
				TestMACsecFilterStateGuard.cpp \
				TestNetMsgRegistrar.cpp \
				TestRealObjectIdManager.cpp \
				TestResourceLimiter.cpp \
				TestResourceLimiterContainer.cpp \
				TestResourceLimiterParser.cpp \
				TestSaiUnittests.cpp \
				TestSelectableFd.cpp \
				TestSignal.cpp \
				TestSwitchConfigContainer.cpp \
				TestTrafficForwarder.cpp \
				TestHostInterfaceInfo.cpp \
				TestTrafficFilterPipes.cpp \
				TestSwitchConfig.cpp \
				TestSwitchContainer.cpp \
				TestSwitch.cpp \
				TestSwitchMLNX2700.cpp \
				TestSwitchNvdaMBF2H536C.cpp \
				TestSwitchBCM56850.cpp \
				TestSwitchBCM81724.cpp \
				TestSwitchStateBaseMACsec.cpp \
				TestMACsecManager.cpp \
				TestSwitchStateBase.cpp \
				TestSai.cpp \
				TestVirtualSwitchSaiInterface.cpp \
				TestTAM.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -fno-access-control
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/vslib/libSaiVS.a -lhiredis -lswsscommon -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 \
			  -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests
