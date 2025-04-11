AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib -I/usr/include/libnl3

if USE_VPP
AM_CXXFLAGS += -Ivpp -DUSE_VPP
endif

CFLAGS_COMMON= -fPIC -pipe -Wall -Wcast-align -Wcast-qual -Wconversion -Wdisabled-optimization -Werror -Wextra -Wfloat-equal -Wformat=2 -Wformat-nonliteral -Wformat-security -Wformat-y2k -Wimport -Winit-self -Wno-inline -Winvalid-pch -Wmissing-field-initializers -Wmissing-format-attribute -Wmissing-include-dirs -Wmissing-noreturn -Wno-aggregate-return -Wno-padded -Wno-switch-enum -Wno-unused-parameter -Wpacked -Wpointer-arith -Wredundant-decls -Wshadow -Wstack-protector -Wstrict-aliasing=3 -Wswitch -Wswitch-default -Wunreachable-code -Wunused -Wvariadic-macros -Wwrite-strings -Wno-switch-default -Wconversion -Wno-psabi -Wcast-align=strict

# TODO fix multiple paths
AM_CFLAGS = -I/usr/include/vpp_plugins $(SAIINC) -fPIC $(CFLAGS_COMMON) -I.

lib_LTLIBRARIES = libsaivs.la

noinst_LIBRARIES = libSaiVS.a

# TODO: move vppxlate to proper lib and namespaces

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

if USE_VPP
libSaiVS_a_SOURCES +=\
					  vpp/SaiObjectDB.cpp \
					  vpp/SwitchVpp.cpp \
					  vpp/SwitchVppAcl.cpp \
					  vpp/SwitchVppNexthop.cpp \
					  vpp/SwitchVppUtils.cpp \
					  vpp/SwitchVppFdb.cpp \
					  vpp/SwitchVppRif.cpp \
					  vpp/SwitchVppBfd.cpp \
					  vpp/SwitchVppHostif.cpp \
					  vpp/SwitchVppRoute.cpp \
					  vpp/SwitchVppNbr.cpp \
					  vpp/SwitchVppSRv6.cpp \
					  vpp/TunnelManager.cpp \
					  vpp/vppxlate/SaiVppXlate.c \
					  vpp/vppxlate/SaiVppStats.c \
					  vpp/vppxlate/SaiAclStats.c \
					  vpp/vppxlate/SaiIntfStats.c
endif

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
libsaivs_la_LIBADD = -lhiredis -lswsscommon libSaiVS.a $(CODE_COVERAGE_LIBS) $(VPP_LIBS)

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread libsaivs.la -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(VPP_LIBS)

TESTS = tests
