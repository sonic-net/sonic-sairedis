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
				TestEventPayloadPlatformEvent.cpp \
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
tests_SOURCES += TestSwitchVpp.cpp TestSwitchVppRif.cpp TestVppInterfaceRegistry.cpp \
				TestSwitchVppPbh.cpp \
				MockVppXlate.cpp MockVppXlate.h \
				SwitchVppTestHarness.cpp SwitchVppTestHarness.h TestSwitchVppNat.cpp
VPP_TEST_WRAPS = vpp_sonic_ext_feature_get vpp_iface_loopback_set_action \
				vpp_intf_stats_query create_bvi_interface delete_bvi_interface \
				create_sub_interface delete_sub_interface configure_lcp_interface \
				set_sw_interface_l2_bridge interface_set_state refresh_interfaces_list \
				vpp_set_mac_event_cb vpp_l2fib_dump vpp_bfd_session_dump vpp_ip_flow_hash_set set_interface_vrf \
				get_interface_vrf ip_vrf_add ip_vrf_del interface_ip_address_del_all \
				init_vpp_client vpp_wait_for_events vpp_drain_events vpp_sync_for_events vpp_ev_dequeue \
				interface_set_promiscuous set_l2_interface_vlan_tag_rewrite set_bridge_domain_flags \
				bridge_domain_get_member_count vpp_bridge_domain_add_del \
				vpp_get_swif_idx_by_name vpp_add_node_next vpp_classify_set_interface_l2_tables \
				sw_interface_set_mtu sw_interface_set_mtu_all get_interface_state interface_get_state \
				sw_interface_set_mpls_enable get_interface_mpls_enabled mpls_table_add_del \
				vpp_pbh_profile_add_del vpp_pbh_table_add_replace vpp_pbh_table_del \
				vpp_pbh_interface_attach_detach vpp_rule_stats_query \
				vpp_nat44_ed_supported vpp_nat44_ed_enable_disable \
				vpp_nat44_forwarding_enable_disable vpp_nat44_ed_output_interface_add_del \
				vpp_nat44_static_mapping_add_del vpp_nat44_user_sessions_dump
tests_LDFLAGS = $(foreach symbol,$(VPP_TEST_WRAPS),-Wl,--wrap=$(symbol))
endif

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -fno-access-control
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/vslib/libSaiVS.a -lhiredis -lswsscommon -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 \
			  -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS) $(VPP_LIBS)

TESTS = tests
