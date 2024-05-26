AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/lib

bin_PROGRAMS = testslibsaistub

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

testslibsaistub_SOURCES = main.cpp \
				../../meta/DummySaiInterface.cpp \
				test_sai_stub_acl.cpp \
				test_sai_stub_ars.cpp \
				test_sai_stub_ars_profile.cpp \
				test_sai_stub_bfd.cpp \
				test_sai_stub_bmtor.cpp \
				test_sai_stub_bridge.cpp \
				test_sai_stub_buffer.cpp \
				test_sai_stub_counter.cpp \
				test_sai_stub_dash_acl.cpp \
				test_sai_stub_dash_direction_lookup.cpp \
				test_sai_stub_dash_eni.cpp \
				test_sai_stub_dash_inbound_routing.cpp \
				test_sai_stub_dash_outbound_ca_to_pa.cpp \
				test_sai_stub_dash_outbound_routing.cpp \
				test_sai_stub_dash_pa_validation.cpp \
				test_sai_stub_dash_vip.cpp \
				test_sai_stub_dash_vnet.cpp \
				test_sai_stub_debug_counter.cpp \
				test_sai_stub_dtel.cpp \
				test_sai_stub_fdb.cpp \
				test_sai_stub_generic_programmable.cpp \
				test_sai_stub_hash.cpp \
				test_sai_stub_hostif.cpp \
				test_sai_stub_interfacequery.cpp \
				test_sai_stub_ipmc.cpp \
				test_sai_stub_ipmc_group.cpp \
				test_sai_stub_isolation_group.cpp \
				test_sai_stub_l2mc.cpp \
				test_sai_stub_l2mcgroup.cpp \
				test_sai_stub_lag.cpp \
				test_sai_stub_macsec.cpp \
				test_sai_stub_mcastfdb.cpp \
				test_sai_stub_mirror.cpp \
				test_sai_stub_mpls.cpp \
				test_sai_stub_nat.cpp \
				test_sai_stub_neighbor.cpp \
				test_sai_stub_nexthop.cpp \
				test_sai_stub_nexthopgroup.cpp \
				test_sai_stub_poe.cpp \
				test_sai_stub_policer.cpp \
				test_sai_stub_port.cpp \
				test_sai_stub_qosmap.cpp \
				test_sai_stub_queue.cpp \
				test_sai_stub_route.cpp \
				test_sai_stub_router_interface.cpp \
				test_sai_stub_router_rpfgroup.cpp \
				test_sai_stub_router_samplepacket.cpp \
				test_sai_stub_scheduler.cpp \
				test_sai_stub_schedulergroup.cpp \
				test_sai_stub_srv6.cpp \
				test_sai_stub_stp.cpp \
				test_sai_stub_switch.cpp \
				test_sai_stub_system_port.cpp \
				test_sai_stub_tam.cpp \
				test_sai_stub_tunnel.cpp \
				test_sai_stub_udf.cpp \
				test_sai_stub_virtual_router.cpp \
				test_sai_stub_vlan.cpp \
				test_sai_stub_wred.cpp

testslibsaistub_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
testslibsaistub_LDADD = $(LDADD_GTEST) $(top_srcdir)/stub/libsaistub.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = testslibsaistub
