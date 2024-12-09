AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib -I/usr/include/libnl3

lib_LTLIBRARIES = libsaivs.la

noinst_LIBRARIES = libSaiVS.a

libSaiVS_a_SOURCES = \
					  Buffer.cpp \
					  ContextConfigContainer.cpp \
					  ContextConfig.cpp \
					  Context.cpp \
					  CorePortIndexMapContainer.cpp \
					  CorePortIndexMap.cpp \
					  CorePortIndexMapFileParser.cpp \
					  Event.cpp \
					  EventPayloadNetLinkMsg.cpp \
					  EventPayloadNotification.cpp \
					  EventPayloadPacket.cpp \
					  EventQueue.cpp \
					  FdbInfo.cpp \
					  HostInterfaceInfo.cpp \
					  LaneMapContainer.cpp \
					  LaneMap.cpp \
					  LaneMapFileParser.cpp \
					  MACsecAttr.cpp \
					  MACsecFilterStateGuard.cpp \
					  MACsecEgressFilter.cpp \
					  MACsecFilter.cpp \
					  MACsecForwarder.cpp \
					  MACsecIngressFilter.cpp \
					  MACsecManager.cpp \
					  NetMsgRegistrar.cpp \
					  RealObjectIdManager.cpp \
					  ResourceLimiterContainer.cpp \
					  ResourceLimiter.cpp \
					  ResourceLimiterParser.cpp \
					  SaiAttrWrap.cpp \
					  Sai.cpp \
					  SaiEventQueue.cpp \
					  SaiFdbAging.cpp \
					  SaiUnittests.cpp \
					  SelectableFd.cpp \
					  Signal.cpp \
					  SwitchBCM56850.cpp \
					  SwitchBCM56971B0.cpp \
					  SwitchBCM81724.cpp \
					  SwitchConfigContainer.cpp \
					  SwitchConfig.cpp \
					  SwitchContainer.cpp \
					  Switch.cpp \
					  SwitchMLNX2700.cpp \
					  SwitchPoE.cpp \
					  SwitchNvdaMBF2H536C.cpp \
					  SwitchStateBase.cpp \
					  SwitchStateBaseFdb.cpp \
					  SwitchStateBaseHostif.cpp \
					  SwitchStateBaseMACsec.cpp \
					  SwitchState.cpp \
					  TrafficFilterPipes.cpp \
					  TrafficForwarder.cpp \
					  VirtualSwitchSaiInterface.cpp \
					  VirtualSwitchSaiInterfaceFdb.cpp \
					  VirtualSwitchSaiInterfacePort.cpp

BUILT_SOURCES = sai_vs.cpp

sai_vs.cpp: ../stub.pl $(top_srcdir)/SAI/meta/saimetadata.c
	../stub.pl -d ../SAI/ -c Sai -n saivs -f sai_vs.cpp -s stub

clean-local:
	rm -f sai_vs.cpp

libsaivs_la_SOURCES = sai_vs.cpp

libSaiVS_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiVS_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

libsaivs_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaivs_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsaivs_la_LIBADD = -lhiredis -lswsscommon libSaiVS.a $(CODE_COVERAGE_LIBS)

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread libsaivs.la -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq

TESTS = tests
