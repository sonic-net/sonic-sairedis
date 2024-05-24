AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib

lib_LTLIBRARIES = libsaiproxy.la

noinst_LIBRARIES = libSaiProxy.a

libSaiProxy_a_SOURCES = \
						 Sai.cpp

libsaiproxy_la_SOURCES = \
						 sai_proxy_acl.cpp \
						 sai_proxy_bfd.cpp \
						 sai_proxy_bmtor.cpp \
						 sai_proxy_bridge.cpp \
						 sai_proxy_buffer.cpp \
						 sai_proxy_counter.cpp \
						 sai_proxy_dash_vip.cpp \
						 sai_proxy_dash_pa_validation.cpp \
						 sai_proxy_dash_vnet.cpp \
						 sai_proxy_dash_outbound_routing.cpp \
						 sai_proxy_dash_outbound_ca_to_pa.cpp \
						 sai_proxy_dash_inbound_routing.cpp \
						 sai_proxy_dash_eni.cpp \
						 sai_proxy_dash_direction_lookup.cpp \
						 sai_proxy_dash_acl.cpp \
						 sai_proxy_debug_counter.cpp \
						 sai_proxy_dtel.cpp \
						 sai_proxy_fdb.cpp \
						 sai_proxy_genericprogrammable.cpp \
						 sai_proxy_hash.cpp \
						 sai_proxy_hostif.cpp \
						 sai_proxy_interfacequery.cpp \
						 sai_proxy_ipmc.cpp \
						 sai_proxy_ipmc_group.cpp \
						 sai_proxy_isolation_group.cpp \
						 sai_proxy_l2mc.cpp \
						 sai_proxy_l2mcgroup.cpp \
						 sai_proxy_lag.cpp \
						 sai_proxy_macsec.cpp \
						 sai_proxy_mcastfdb.cpp \
						 sai_proxy_mirror.cpp \
						 sai_proxy_mpls.cpp \
						 sai_proxy_nat.cpp \
						 sai_proxy_neighbor.cpp \
						 sai_proxy_nexthop.cpp \
						 sai_proxy_nexthopgroup.cpp \
						 sai_proxy_policer.cpp \
						 sai_proxy_port.cpp \
						 sai_proxy_qosmap.cpp \
						 sai_proxy_queue.cpp \
						 sai_proxy_route.cpp \
						 sai_proxy_router_interface.cpp \
						 sai_proxy_rpfgroup.cpp \
						 sai_proxy_samplepacket.cpp \
						 sai_proxy_scheduler.cpp \
						 sai_proxy_schedulergroup.cpp \
						 sai_proxy_srv6.cpp \
						 sai_proxy_stp.cpp \
						 sai_proxy_switch.cpp \
						 sai_proxy_system_port.cpp \
						 sai_proxy_tam.cpp \
						 sai_proxy_tunnel.cpp \
						 sai_proxy_udf.cpp \
						 sai_proxy_virtual_router.cpp \
						 sai_proxy_vlan.cpp \
						 sai_proxy_wred.cpp \
						 sai_proxy_ipsec.cpp \
						 sai_proxy_my_mac.cpp \
						 sai_proxy_ars.cpp \
						 sai_proxy_ars_profile.cpp \
						 sai_proxy_twamp.cpp \
						 sai_proxy_poe.cpp \
						 sai_proxy_dash_meter.cpp

libSaiProxy_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiProxy_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

libsaiproxy_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaiproxy_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsaiproxy_la_LIBADD = -lhiredis -lswsscommon libSaiProxy.a $(CODE_COVERAGE_LIBS)

# TODO - lib sai proxy server that will accept VendorSai

