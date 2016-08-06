# Makefile.am -- Process this file with automake to produce Makefile.in

AM_CPPFLAGS =
AM_CPPFLAGS += -I/usr/include/sai -I/usr/include/swss -I$(top_srcdir)/common
AM_CPPFLAGS += -I../inc

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

lib_LTLIBRARIES = libsaivs.la

libsaivs_la_SOURCES = sai_vs_acl.cpp \
			 sai_vs_buffer.cpp \
			 sai_vs_fdb.cpp \
			 sai_vs_hash.cpp \
			 sai_vs_hostintf.cpp \
			 sai_vs_interfacequery.cpp \
			 sai_vs_lag.cpp \
			 sai_vs_mirror.cpp \
			 sai_vs_neighbor.cpp \
			 sai_vs_nexthop.cpp \
			 sai_vs_nexthopgroup.cpp \
			 sai_vs_policer.cpp \
			 sai_vs_port.cpp \
			 sai_vs_qosmaps.cpp \
			 sai_vs_queue.cpp \
			 sai_vs_route.cpp \
			 sai_vs_router.cpp \
			 sai_vs_routerintf.cpp \
			 sai_vs_samplepacket.cpp \
			 sai_vs_scheduler.cpp \
			 sai_vs_schedulergroup.cpp \
			 sai_vs_stp.cpp \
			 sai_vs_switch.cpp \
			 sai_vs_tunnel.cpp \
			 sai_vs_udf.cpp \
			 sai_vs_vlan.cpp \
			 sai_vs_wred.cpp \
			 sai_vs_generic_create.cpp \
			 sai_vs_generic_remove.cpp \
			 sai_vs_generic_set.cpp \
			 sai_vs_generic_get.cpp \
			 ../../common/saiserialize.cpp \
			 ../../common/saiattributelist.cpp


libsaivs_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)

libsaivs_la_LIBADD = -lhiredis -lswsscommon
