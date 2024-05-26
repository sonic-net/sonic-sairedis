AM_CXXFLAGS = $(SAIINC)

noinst_LIBRARIES = libsaistub.a

libsaistub_a_SOURCES = \
						 sai_stub_acl.cpp \
						 sai_stub_ars.cpp \
						 sai_stub_ars_profile.cpp \
						 sai_stub_bfd.cpp \
						 sai_stub_bmtor.cpp \
						 sai_stub_bridge.cpp \
						 sai_stub_buffer.cpp \
						 sai_stub_counter.cpp \
						 sai_stub_dash_acl.cpp \
						 sai_stub_dash_direction_lookup.cpp \
						 sai_stub_dash_eni.cpp \
						 sai_stub_dash_inbound_routing.cpp \
						 sai_stub_dash_meter.cpp \
						 sai_stub_dash_outbound_ca_to_pa.cpp \
						 sai_stub_dash_outbound_routing.cpp \
						 sai_stub_dash_pa_validation.cpp \
						 sai_stub_dash_vip.cpp \
						 sai_stub_dash_vnet.cpp \
						 sai_stub_debug_counter.cpp \
						 sai_stub_dtel.cpp \
						 sai_stub_fdb.cpp \
						 sai_stub_genericprogrammable.cpp \
						 sai_stub_hash.cpp \
						 sai_stub_hostif.cpp \
						 sai_stub_interfacequery.cpp \
						 sai_stub_ipmc.cpp \
						 sai_stub_ipmc_group.cpp \
						 sai_stub_ipsec.cpp \
						 sai_stub_isolation_group.cpp \
						 sai_stub_l2mc.cpp \
						 sai_stub_l2mcgroup.cpp \
						 sai_stub_lag.cpp \
						 sai_stub_macsec.cpp \
						 sai_stub_mcastfdb.cpp \
						 sai_stub_mirror.cpp \
						 sai_stub_mpls.cpp \
						 sai_stub_my_mac.cpp \
						 sai_stub_nat.cpp \
						 sai_stub_neighbor.cpp \
						 sai_stub_nexthop.cpp \
						 sai_stub_nexthopgroup.cpp \
						 sai_stub_poe.cpp \
						 sai_stub_policer.cpp \
						 sai_stub_port.cpp \
						 sai_stub_qosmap.cpp \
						 sai_stub_queue.cpp \
						 sai_stub_route.cpp \
						 sai_stub_router_interface.cpp \
						 sai_stub_rpfgroup.cpp \
						 sai_stub_samplepacket.cpp \
						 sai_stub_scheduler.cpp \
						 sai_stub_schedulergroup.cpp \
						 sai_stub_srv6.cpp \
						 sai_stub_stp.cpp \
						 sai_stub_switch.cpp \
						 sai_stub_system_port.cpp \
						 sai_stub_tam.cpp \
						 sai_stub_tunnel.cpp \
						 sai_stub_twamp.cpp \
						 sai_stub_udf.cpp \
						 sai_stub_virtual_router.cpp \
						 sai_stub_vlan.cpp \
						 sai_stub_wred.cpp

libsaistub_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaistub_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp ../meta/DummySaiInterface.cpp
tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lswsscommon -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq libsaistub.a #-lhiredis -lpthread -lsaimetadata -lsaimeta libsaistub.la -lzmq

TESTS = tests
