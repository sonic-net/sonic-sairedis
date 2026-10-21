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
VPP_TEST_WRAPS = vpp_sonic_ext_feature_get vpp_iface_loopback_set_action \
				vpp_intf_stats_query create_bvi_interface delete_bvi_interface \
				create_sub_interface delete_sub_interface configure_lcp_interface \
				set_sw_interface_l2_bridge interface_set_state refresh_interfaces_list \
				vpp_want_l2_macs_events2 vpp_ip_flow_hash_set set_interface_vrf \
				get_interface_vrf ip_vrf_add ip_vrf_del interface_ip_address_del_all \
				init_vpp_client vpp_sync_for_events vpp_ev_dequeue interface_set_promiscuous \
				set_l2_interface_vlan_tag_rewrite set_bridge_domain_flags \
				bridge_domain_get_member_count vpp_bridge_domain_add_del \
				vpp_get_swif_idx_by_name vpp_add_node_next vpp_classify_set_interface_l2_tables \
				_ZN5saivs9SwitchVpp17vpp_dp_initializeEv \
				_ZN5saivs9SwitchVpp19vs_set_dev_admin_upEPKcb \
				_ZN4swss4execERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEERS5_
tests_LDFLAGS = $(foreach symbol,$(VPP_TEST_WRAPS),-Wl,--wrap=$(symbol))
endif

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) -fno-access-control
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/vslib/libSaiVS.a -lhiredis -lswsscommon -lnl-genl-3 -lnl-nf-3 -lnl-route-3 -lnl-3 \
			  -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS) $(VPP_LIBS)

TESTS = tests
