AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/proxylib -I$(top_srcdir)/lib

bin_PROGRAMS = tests testslibsaiproxy

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = \
				main.cpp \
				TestSai.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/proxylib/libSaiProxy.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

testslibsaiproxy_SOURCES =	main_libsaiproxy.cpp \
				test_sai_proxy_acl.cpp \
				test_sai_proxy_bfd.cpp \
				test_sai_proxy_bmtor.cpp \
				test_sai_proxy_bridge.cpp \
				test_sai_proxy_buffer.cpp \
				test_sai_proxy_counter.cpp \
				test_sai_proxy_dash_vip.cpp \
				test_sai_proxy_dash_pa_validation.cpp \
				test_sai_proxy_dash_vnet.cpp \
				test_sai_proxy_dash_outbound_routing.cpp \
				test_sai_proxy_dash_outbound_ca_to_pa.cpp \
				test_sai_proxy_dash_inbound_routing.cpp \
				test_sai_proxy_dash_eni.cpp \
				test_sai_proxy_dash_direction_lookup.cpp \
				test_sai_proxy_dash_acl.cpp \
				test_sai_proxy_debug_counter.cpp \
				test_sai_proxy_dtel.cpp \
				test_sai_proxy_fdb.cpp \
				test_sai_proxy_generic_programmable.cpp \
				test_sai_proxy_ipmc.cpp \
				test_sai_proxy_l2mc.cpp \
				test_sai_proxy_l2mcgroup.cpp \
				test_sai_proxy_lag.cpp \
				test_sai_proxy_ipmc_group.cpp \
				test_sai_proxy_macsec.cpp \
				test_sai_proxy_isolation_group.cpp \
				test_sai_proxy_interfacequery.cpp \
				test_sai_proxy_mcastfdb.cpp \
				test_sai_proxy_mirror.cpp \
				test_sai_proxy_mpls.cpp \
				test_sai_proxy_nat.cpp \
				test_sai_proxy_hash.cpp \
				test_sai_proxy_neighbor.cpp \
				test_sai_proxy_nexthop.cpp \
				test_sai_proxy_nexthopgroup.cpp \
				test_sai_proxy_port.cpp \
				test_sai_proxy_qosmap.cpp \
				test_sai_proxy_policer.cpp \
				test_sai_proxy_queue.cpp \
				test_sai_proxy_route.cpp \
				test_sai_proxy_router_interface.cpp \
				test_sai_proxy_router_rpfgroup.cpp \
				test_sai_proxy_router_samplepacket.cpp \
				test_sai_proxy_schedulergroup.cpp \
				test_sai_proxy_scheduler.cpp \
				test_sai_proxy_srv6.cpp \
				test_sai_proxy_switch.cpp \
				test_sai_proxy_system_port.cpp \
				test_sai_proxy_tam.cpp \
				test_sai_proxy_tunnel.cpp \
				test_sai_proxy_stp.cpp \
				test_sai_proxy_udf.cpp \
				test_sai_proxy_virtual_router.cpp \
				test_sai_proxy_vlan.cpp \
				test_sai_proxy_hostif.cpp \
				test_sai_proxy_wred.cpp \
				test_sai_proxy_ars.cpp \
				test_sai_proxy_poe.cpp \
				test_sai_proxy_ars_profile.cpp

testslibsaiproxy_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
testslibsaiproxy_LDADD = $(LDADD_GTEST) -L$(top_srcdir)/proxylib/.libs -lsaiproxy -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = testslibsaiproxy tests
