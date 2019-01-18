AM_CPPFLAGS = -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I/usr/include/libnl3 -I$(top_srcdir)/SAI/experimental

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

lib_LTLIBRARIES = libsaivs.la

libsaivs_la_SOURCES = \
					  sai_vs_acl.cpp \
					  sai_vs_bfd.cpp \
					  sai_vs_bridge.cpp \
					  sai_vs_buffer.cpp \
					  sai_vs_dtel.cpp \
					  sai_vs_fdb.cpp \
					  sai_vs_hash.cpp \
					  sai_vs_hostintf.cpp \
					  sai_vs_interfacequery.cpp \
					  sai_vs_ipmc.cpp \
					  sai_vs_ipmc_group.cpp \
					  sai_vs_l2mc.cpp \
					  sai_vs_l2mcgroup.cpp \
					  sai_vs_lag.cpp \
					  sai_vs_mcastfdb.cpp \
					  sai_vs_mirror.cpp \
					  sai_vs_mpls.cpp \
					  sai_vs_neighbor.cpp \
					  sai_vs_nexthop.cpp \
					  sai_vs_nexthopgroup.cpp \
					  sai_vs_policer.cpp \
					  sai_vs_port.cpp \
					  sai_vs_qosmaps.cpp \
					  sai_vs_queue.cpp \
					  sai_vs_route.cpp \
					  sai_vs_router_interface.cpp \
					  sai_vs_rpfgroup.cpp \
					  sai_vs_samplepacket.cpp \
					  sai_vs_scheduler.cpp \
					  sai_vs_schedulergroup.cpp \
					  sai_vs_segmentroute.cpp \
					  sai_vs_stp.cpp \
					  sai_vs_switch.cpp \
					  sai_vs_tam.cpp \
					  sai_vs_tunnel.cpp \
					  sai_vs_uburst.cpp \
					  sai_vs_udf.cpp \
					  sai_vs_virtual_router.cpp \
					  sai_vs_vlan.cpp \
					  sai_vs_wred.cpp \
					  sai_vs_generic_create.cpp \
					  sai_vs_generic_get.cpp \
					  sai_vs_generic_remove.cpp \
					  sai_vs_generic_set.cpp \
					  sai_vs_generic_stats.cpp \
					  sai_vs.cpp \
					  sai_vs_switch_BCM56850.cpp \
					  sai_vs_switch_MLNX2700.cpp

libsaivs_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
libsaivs_la_LIBADD = -lhiredis -lswsscommon

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/vslib/src/.libs -lsaivs -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta

TESTS = tests
