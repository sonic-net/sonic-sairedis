AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib/inc

lib_LTLIBRARIES = libsairedis.la

noinst_LIBRARIES = libSaiRedis.a

libSaiRedis_a_SOURCES = \
						 Channel.cpp \
						 ClientConfig.cpp \
						 ClientSai.cpp \
						 ClientServerSai.cpp \
						 Context.cpp \
						 ContextConfig.cpp \
						 ContextConfigContainer.cpp \
						 Recorder.cpp \
						 RedisChannel.cpp \
						 RedisRemoteSaiInterface.cpp \
						 RedisVidIndexGenerator.cpp \
						 Sai.cpp \
						 ServerConfig.cpp \
						 ServerSai.cpp \
						 SkipRecordAttrContainer.cpp \
						 Switch.cpp \
						 SwitchConfig.cpp \
						 SwitchConfigContainer.cpp \
						 SwitchContainer.cpp \
						 Utils.cpp \
						 VirtualObjectIdManager.cpp \
						 ZeroMQChannel.cpp

libsairedis_la_SOURCES = \
						 sai_redis_acl.cpp \
						 sai_redis_bfd.cpp \
						 sai_redis_bmtor.cpp \
						 sai_redis_bridge.cpp \
						 sai_redis_buffer.cpp \
						 sai_redis_counter.cpp \
						 sai_redis_debug_counter.cpp \
						 sai_redis_dtel.cpp \
						 sai_redis_fdb.cpp \
						 sai_redis_hash.cpp \
						 sai_redis_hostintf.cpp \
						 sai_redis_interfacequery.cpp \
						 sai_redis_ipmc.cpp \
						 sai_redis_ipmc_group.cpp \
						 sai_redis_isolation_group.cpp \
						 sai_redis_l2mc.cpp \
						 sai_redis_l2mcgroup.cpp \
						 sai_redis_lag.cpp \
						 sai_redis_macsec.cpp \
						 sai_redis_mcastfdb.cpp \
						 sai_redis_mirror.cpp \
						 sai_redis_mpls.cpp \
						 sai_redis_nat.cpp \
						 sai_redis_neighbor.cpp \
						 sai_redis_nexthop.cpp \
						 sai_redis_nexthopgroup.cpp \
						 sai_redis_policer.cpp \
						 sai_redis_port.cpp \
						 sai_redis_qosmaps.cpp \
						 sai_redis_queue.cpp \
						 sai_redis_route.cpp \
						 sai_redis_router_interface.cpp \
						 sai_redis_rpfgroup.cpp \
						 sai_redis_samplepacket.cpp \
						 sai_redis_scheduler.cpp \
						 sai_redis_schedulergroup.cpp \
						 sai_redis_segmentroute.cpp \
						 sai_redis_stp.cpp \
						 sai_redis_switch.cpp \
						 sai_redis_system_port.cpp \
						 sai_redis_tam.cpp \
						 sai_redis_tunnel.cpp \
						 sai_redis_udf.cpp \
						 sai_redis_virtual_router.cpp \
						 sai_redis_vlan.cpp \
						 sai_redis_wred.cpp

libSaiRedis_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiRedis_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

libsairedis_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsairedis_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsairedis_la_LIBADD = -lhiredis -lswsscommon libSaiRedis.a $(CODE_COVERAGE_LIBS)

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta libsairedis.la -lzmq

TESTS = tests
