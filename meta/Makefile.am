AM_CPPFLAGS = -I/usr/include/sai

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

lib_LTLIBRARIES = libsaimetadata.la

libsaimetadata_la_SOURCES = \
							sai_extra_acl.cpp \
							sai_extra_buffer.cpp \
							sai_extra_fdb.cpp \
							sai_extra_hash.cpp \
							sai_extra_hostintf.cpp \
							sai_extra_lag.cpp \
							sai_extra_mirror.cpp \
							sai_extra_neighbor.cpp \
							sai_extra_nexthopgroup.cpp \
							sai_extra_nexthop.cpp \
							sai_extra_policer.cpp \
							sai_extra_port.cpp \
							sai_extra_qosmaps.cpp \
							sai_extra_queue.cpp \
							sai_extra_route.cpp \
							sai_extra_routerintf.cpp \
							sai_extra_router.cpp \
							sai_extra_samplepacket.cpp \
							sai_extra_schedulergroup.cpp \
							sai_extra_scheduler.cpp \
							sai_extra_stp.cpp \
							sai_extra_switch.cpp \
							sai_extra_tunnel.cpp \
							sai_extra_udf.cpp \
							sai_extra_vlan.cpp \
							sai_extra_wred.cpp \
							sai_meta_acl.cpp \
							sai_meta_buffer.cpp \
							sai_meta_fdb.cpp \
							sai_meta_hash.cpp \
							sai_meta_hostintf.cpp \
							sai_meta_lag.cpp \
							sai_meta_mirror.cpp \
							sai_meta_neighbor.cpp \
							sai_meta_nexthopgroup.cpp \
							sai_meta_nexthop.cpp \
							sai_meta.cpp \
							sai_meta_policer.cpp \
							sai_meta_port.cpp \
							sai_meta_qosmaps.cpp \
							sai_meta_queue.cpp \
							sai_meta_route.cpp \
							sai_meta_routerintf.cpp \
							sai_meta_router.cpp \
							sai_meta_samplepacket.cpp \
							sai_meta_sanity.cpp \
							sai_meta_schedulergroup.cpp \
							sai_meta_scheduler.cpp \
							sai_meta_stp.cpp \
							sai_meta_switch.cpp \
							sai_meta_tunnel.cpp \
							sai_meta_udf.cpp \
							sai_meta_vlan.cpp \
							sai_meta_wred.cpp \
							saiattributelist.cpp \
							saiserialize.cpp

libsaimetadata_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
libsaimetadata_la_LIBADD = -lhiredis -lswsscommon

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) 
tests_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_dir)/meta/.libs -lsaimetadata

TESTS = tests
#.PHONY: runtests
#
#runtests:
#	./tests
