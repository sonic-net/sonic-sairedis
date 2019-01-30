AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

lib_LTLIBRARIES = libsairedis.la

libsairedis_la_SOURCES = \
						 sai_redis_acl.cpp \
						 sai_redis_bfd.cpp \
						 sai_redis_bmtor.cpp \
						 sai_redis_bridge.cpp \
						 sai_redis_buffer.cpp \
						 sai_redis_dtel.cpp \
						 sai_redis_fdb.cpp \
						 sai_redis_hash.cpp \
						 sai_redis_hostintf.cpp \
						 sai_redis_interfacequery.cpp \
						 sai_redis_ipmc.cpp \
						 sai_redis_ipmc_group.cpp \
						 sai_redis_l2mc.cpp \
						 sai_redis_l2mcgroup.cpp \
						 sai_redis_lag.cpp \
						 sai_redis_mcastfdb.cpp \
						 sai_redis_mirror.cpp \
						 sai_redis_mpls.cpp \
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
						 sai_redis_tam.cpp \
						 sai_redis_tunnel.cpp \
						 sai_redis_uburst.cpp \
						 sai_redis_udf.cpp \
						 sai_redis_virtual_router.cpp \
						 sai_redis_vlan.cpp \
						 sai_redis_wred.cpp \
						 sai_redis_generic_create.cpp \
						 sai_redis_generic_remove.cpp \
						 sai_redis_generic_set.cpp \
						 sai_redis_generic_get.cpp \
						 sai_redis_generic_stats.cpp \
						 sai_redis_notifications.cpp \
						 sai_redis_record.cpp

libsairedis_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
libsairedis_la_LIBADD = -lhiredis -lswsscommon
