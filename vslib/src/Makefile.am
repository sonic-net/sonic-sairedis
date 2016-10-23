WARNINGS = \
		   -ansi \
		   -Wall \
		   -Wcast-align \
		   -Wcast-qual \
		   -Wconversion \
		   -Wdisabled-optimization \
		   -Werror \
		   -Wextra \
		   -Wextra \
		   -Wfloat-equal \
		   -Wformat=2 \
		   -Wformat-nonliteral \
		   -Wformat-security \
		   -Wformat-y2k \
		   -Wimport \
		   -Winit-self \
		   -Winline \
		   -Winvalid-pch \
		   -Wlong-long \
		   -Wmissing-field-initializers \
		   -Wmissing-format-attribute \
		   -Wmissing-include-dirs \
		   -Wmissing-noreturn \
		   -Wno-aggregate-return \
		   -Wno-padded \
		   -Wno-switch-enum \
		   -Wno-unused-parameter \
		   -Wpacked \
		   -Wpointer-arith \
		   -Wredundant-decls \
		   -Wshadow \
		   -Wstack-protector \
		   -Wstrict-aliasing=3 \
		   -Wswitch \
		   -Wswitch-default \
		   -Wunreachable-code \
		   -Wunused \
		   -Wvariadic-macros \
		   -Wwrite-strings

AM_CPPFLAGS =
AM_CPPFLAGS += -I/usr/include/sai -I/usr/include/swss -I$(top_srcdir)/common
AM_CPPFLAGS += -I../inc -I../../meta $(WARNINGS)

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

lib_LTLIBRARIES = libsai.la

libsai_la_SOURCES = \
					../../meta/saiattributelist.cpp \
					../../meta/saiserialize.cpp \
					../../meta/sai_meta_acl.cpp \
					../../meta/sai_meta_buffer.cpp \
					../../meta/sai_meta.cpp \
					../../meta/sai_meta_fdb.cpp \
					../../meta/sai_meta_hash.cpp \
					../../meta/sai_meta_hostintf.cpp \
					../../meta/sai_meta_lag.cpp \
					../../meta/sai_meta_mirror.cpp \
					../../meta/sai_meta_neighbor.cpp \
					../../meta/sai_meta_nexthop.cpp \
					../../meta/sai_meta_nexthopgroup.cpp \
					../../meta/sai_meta_policer.cpp \
					../../meta/sai_meta_port.cpp \
					../../meta/sai_meta_qosmaps.cpp \
					../../meta/sai_meta_queue.cpp \
					../../meta/sai_meta_route.cpp \
					../../meta/sai_meta_router.cpp \
					../../meta/sai_meta_routerintf.cpp \
					../../meta/sai_meta_samplepacket.cpp \
					../../meta/sai_meta_sanity.cpp \
					../../meta/sai_meta_scheduler.cpp \
					../../meta/sai_meta_schedulergroup.cpp \
					../../meta/sai_meta_stp.cpp \
					../../meta/sai_meta_switch.cpp \
					../../meta/sai_meta_tunnel.cpp \
					../../meta/sai_meta_udf.cpp \
					../../meta/sai_meta_vlan.cpp \
					../../meta/sai_meta_wred.cpp \
					sai_vs_acl.cpp \
					sai_vs_buffer.cpp \
					sai_vs_fdb.cpp \
					sai_vs_generic_create.cpp \
					sai_vs_generic_get.cpp \
					sai_vs_generic_remove.cpp \
					sai_vs_generic_set.cpp \
					sai_vs_hash.cpp \
					sai_vs_hostintf.cpp \
					sai_vs_init.cpp \
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
					sai_vs_wred.cpp

libsai_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)

libsai_la_LIBADD = -lhiredis -lswsscommon
