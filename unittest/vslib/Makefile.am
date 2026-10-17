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
				TestPortConfigFileParser.cpp \
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
				TestSwitchMicas48CON2SFP.cpp \
				TestSwitchNokiaVS.cpp \
				TestSwitchNvdaMBF2H536C.cpp \
				TestSwitchBCM56971B0.cpp \
				TestSwitchBCM56850.cpp \
				TestSwitchBCM81724.cpp \
				TestSwitchStateBaseMACsec.cpp \
				TestMACsecManager.cpp \
				TestSwitchStateBase.cpp \
				TestSai.cpp \
				TestVirtualSwitchSaiInterface.cpp \
				TestTAM.cpp

if USE_VPP
tests_SOURCES += TestSwitchVpp.cpp

# TestSwitchVpp.cpp replaces these VPP API calls with recording stubs, so the
# SwitchVpp code paths under test run without a VPP instance.
tests_LDFLAGS = \
				-Wl,--wrap=init_vpp_client \
				-Wl,--wrap=vpp_sync_for_events \
				-Wl,--wrap=vpp_ev_dequeue \
				-Wl,--wrap=vpp_want_l2_macs_events2 \
				-Wl,--wrap=refresh_interfaces_list \
				-Wl,--wrap=ip_vrf_add \
				-Wl,--wrap=ip_vrf_del \
				-Wl,--wrap=vpp_ip_flow_hash_set \
				-Wl,--wrap=set_interface_vrf \
				-Wl,--wrap=create_bvi_interface \
				-Wl,--wrap=delete_bvi_interface \
				-Wl,--wrap=set_sw_interface_l2_bridge \
				-Wl,--wrap=set_sw_interface_l2_bridge_by_index \
				-Wl,--wrap=interface_set_state \
				-Wl,--wrap=sw_interface_set_mac_by_index \
				-Wl,--wrap=configure_lcp_interface \
				-Wl,--wrap=interface_ip_address_add_del \
				-Wl,--wrap=vpp_vxlan_tunnel_add_del \
				-Wl,--wrap=ip4_nbr_add_del \
				-Wl,--wrap=ip6_nbr_add_del \
				-Wl,--wrap=vpp_bridge_domain_add_del
endif

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -fno-access-control
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/vslib/libSaiVS.a -lhiredis -lswsscommon -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 \
			  -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS) $(VPP_LIBS)

TESTS = tests
