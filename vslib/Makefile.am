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
					  SwitchBCM81724.cpp \
					  SwitchConfigContainer.cpp \
					  SwitchConfig.cpp \
					  SwitchContainer.cpp \
					  Switch.cpp \
					  SwitchMLNX2700.cpp \
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

libsaivs_la_SOURCES = \
					  sai_vs_acl.cpp \
					  sai_vs_bfd.cpp \
					  sai_vs_bmtor.cpp \
					  sai_vs_bridge.cpp \
					  sai_vs_buffer.cpp \
					  sai_vs_counter.cpp \
					  sai_vs_debug_counter.cpp\
					  sai_vs_dtel.cpp \
					  sai_vs_fdb.cpp \
					  sai_vs_hash.cpp \
					  sai_vs_hostif.cpp \
					  sai_vs_interfacequery.cpp \
					  sai_vs_ipmc.cpp \
					  sai_vs_ipmc_group.cpp \
					  sai_vs_isolation_group.cpp \
					  sai_vs_l2mc.cpp \
					  sai_vs_l2mcgroup.cpp \
					  sai_vs_lag.cpp \
					  sai_vs_macsec.cpp \
					  sai_vs_mcastfdb.cpp \
					  sai_vs_mirror.cpp \
					  sai_vs_mpls.cpp \
					  sai_vs_nat.cpp \
					  sai_vs_neighbor.cpp \
					  sai_vs_nexthop.cpp \
					  sai_vs_nexthopgroup.cpp \
					  sai_vs_policer.cpp \
					  sai_vs_port.cpp \
					  sai_vs_qosmap.cpp \
					  sai_vs_queue.cpp \
					  sai_vs_route.cpp \
					  sai_vs_router_interface.cpp \
					  sai_vs_rpfgroup.cpp \
					  sai_vs_samplepacket.cpp \
					  sai_vs_scheduler.cpp \
					  sai_vs_schedulergroup.cpp \
					  sai_vs_segmentroute.cpp \
					  sai_vs_stp.cpp \
					  sai_vs_switch.cpp \
					  sai_vs_system_port.cpp \
					  sai_vs_tam.cpp \
					  sai_vs_tunnel.cpp \
					  sai_vs_udf.cpp \
					  sai_vs_virtual_router.cpp \
					  sai_vs_vlan.cpp \
					  sai_vs_wred.cpp

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
