AM_CPPFLAGS = -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I/usr/include/libnl3 -I$(top_srcdir)/SAI/experimental -I$(top_srcdir)/lib/inc

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

lib_LTLIBRARIES = libsaivs.la

noinst_LIBRARIES = libSaiVS.a
libSaiVS_a_SOURCES = \
					  ../../lib/src/NotificationFdbEvent.cpp \
					  ../../lib/src/Notification.cpp \
					  ../../lib/src/NotificationPortStateChange.cpp \
					  ../../lib/src/PerformanceIntervalTimer.cpp \
					  ResourceLimiter.cpp \
					  ResourceLimiterContainer.cpp \
					  ResourceLimiterParser.cpp \
					  EventPayloadNotification.cpp \
					  EventPayloadNetLinkMsg.cpp \
					  SaiEventQueue.cpp \
					  Event.cpp \
					  EventQueue.cpp \
					  Signal.cpp \
					  Buffer.cpp \
					  EventPayloadPacket.cpp \
					  VirtualSwitchSaiInterface.cpp \
					  SaiFdbAging.cpp \
					  SwitchConfigContainer.cpp \
					  SwitchConfig.cpp \
					  SaiUnittests.cpp \
					  Sai.cpp \
					  SwitchStateBaseFdb.cpp \
					  SwitchStateBaseHostif.cpp \
					  HostInterfaceInfo.cpp \
					  SwitchStateBase.cpp \
					  NetMsgRegistrar.cpp \
					  LaneMap.cpp \
					  LaneMapContainer.cpp \
					  LaneMapFileParser.cpp \
					  VirtualSwitchSaiInterfacePort.cpp \
					  VirtualSwitchSaiInterfaceFdb.cpp \
					  Switch.cpp \
					  FdbInfo.cpp \
					  SwitchContainer.cpp \
					  RealObjectIdManager.cpp \
					  SaiAttrWrap.cpp \
					  SelectableFd.cpp \
					  SwitchState.cpp \
					  SwitchBCM56850.cpp \
					  SwitchBCM81724.cpp \
					  SwitchMLNX2700.cpp \
					  CorePortIndexMap.cpp \
					  CorePortIndexMapContainer.cpp \
					  CorePortIndexMapFileParser.cpp \
					  MACsecFilter.cpp \
					  MACsecEgressFilter.cpp \
					  MACsecIngressFilter.cpp \
					  TrafficForwarder.cpp \
					  MACsecForwarder.cpp \
					  TrafficFilterPipes.cpp

libsaivs_la_SOURCES = \
					  sai_vs_fdb.cpp \
					  sai_vs_hostintf.cpp \
					  sai_vs_interfacequery.cpp \
					  sai_vs_port.cpp \
					  sai_vs_switch.cpp \
					  sai_vs_debug_counter.cpp\
					  sai_vs_acl.cpp \
					  sai_vs_bfd.cpp \
					  sai_vs_bmtor.cpp \
					  sai_vs_bridge.cpp \
					  sai_vs_buffer.cpp \
					  sai_vs_counter.cpp \
					  sai_vs.cpp \
					  sai_vs_dtel.cpp \
					  sai_vs_hash.cpp \
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
					  sai_vs_qosmaps.cpp \
					  sai_vs_queue.cpp \
					  sai_vs_route.cpp \
					  sai_vs_router_interface.cpp \
					  sai_vs_rpfgroup.cpp \
					  sai_vs_samplepacket.cpp \
					  sai_vs_scheduler.cpp \
					  sai_vs_schedulergroup.cpp \
					  sai_vs_segmentroute.cpp \
					  sai_vs_stp.cpp \
					  sai_vs_system_port.cpp \
					  sai_vs_tam.cpp \
					  sai_vs_tunnel.cpp \
					  sai_vs_udf.cpp \
					  sai_vs_virtual_router.cpp \
					  sai_vs_vlan.cpp \
					  sai_vs_wred.cpp

libSaiVS_a_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)

libsaivs_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
libsaivs_la_LIBADD = -lhiredis -lswsscommon libSaiVS.a

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread libsaivs.la $(top_srcdir)/meta/libsaimetadata.la $(top_srcdir)/meta/libsaimeta.la

TESTS = tests
