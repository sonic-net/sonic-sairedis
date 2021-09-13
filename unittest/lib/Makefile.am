AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/lib

bin_PROGRAMS = tests testslibsairedis

LDADD_GTEST = -L/usr/src/gtest -lgtest -lgtest_main

tests_SOURCES = \
				main.cpp \
				../../meta/NumberOidIndexGenerator.cpp \
				TestSwitch.cpp \
				TestClientConfig.cpp \
				TestClientServerSai.cppa \
				TestContext.cpp \
				TestContextConfig.cpp \
				TestContextConfigContainer.cpp \
				TestUtils.cpp \
				TestVirtualObjectIdManager.cpp \
				TestZeroMQChannel.cpp \
				TestSwitchContainer.cpp \
				TestSwitchConfigContainer.cpp \
				TestSkipRecordAttrContainer.cpp \
				TestServerConfig.cpp \
				TestRedisVidIndexGenerator.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = $(LDADD_GTEST) $(top_srcdir)/lib/libSaiRedis.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

testslibsairedis_SOURCES =	main_libsairedis.cpp \
				test_sai_redis_acl.cpp \
				test_sai_redis_bfd.cpp \
				test_sai_redis_bmtor.cpp \
				test_sai_redis_bridge.cpp \
				test_sai_redis_buffer.cpp \
				test_sai_redis_counter.cpp \
				test_sai_redis_debug_counter.cpp \
				test_sai_redis_dtel.cpp \
				test_sai_redis_fdb.cpp \
				test_sai_redis_ipmc.cpp \
				test_sai_redis_l2mc.cpp \
				test_sai_redis_l2mcgroup.cpp \
				test_sai_redis_lag.cpp \
				test_sai_redis_ipmc_group.cpp \
				test_sai_redis_macsec.cpp \
				test_sai_redis_isolation_group.cpp \
				test_sai_redis_interfacequery.cpp \
				test_sai_redis_mcastfdb.cpp \
				test_sai_redis_mirror.cpp \
				test_sai_redis_mpls.cpp \
				test_sai_redis_nat.cpp \
				test_sai_redis_hash.cpp \
				test_sai_redis_neighbor.cpp \
				test_sai_redis_nexthop.cpp \
				test_sai_redis_nexthopgroup.cpp \
				test_sai_redis_port.cpp \
				test_sai_redis_qosmap.cpp \
				test_sai_redis_policer.cpp \
				test_sai_redis_queue.cpp \
				test_sai_redis_route.cpp \
				test_sai_redis_router_interface.cpp \
				test_sai_redis_router_rpfgroup.cpp \
				test_sai_redis_router_samplepacket.cpp \
				test_sai_redis_schedulergroup.cpp \
				test_sai_redis_scheduler.cpp \
				test_sai_redis_segmentroute.cpp \
				test_sai_redis_switch.cpp \
				test_sai_redis_system_port.cpp \
				test_sai_redis_tam.cpp \
				test_sai_redis_tunnel.cpp \
				test_sai_redis_stp.cpp \
				test_sai_redis_udf.cpp \
				test_sai_redis_virtual_router.cpp \
				test_sai_redis_vlan.cpp \
				test_sai_redis_hostif.cpp \
				test_sai_redis_wred.cpp

testslibsairedis_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
testslibsairedis_LDADD = $(LDADD_GTEST) -L$(top_srcdir)/lib/.libs -lsairedis -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = testslibsairedis tests
