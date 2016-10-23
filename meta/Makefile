warnings = \
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
				-Wstrict-aliasing=2 \
				-Wswitch \
				-Wswitch-default \
				-Wunreachable-code \
				-Wunused \
				-Wvariadic-macros \
				-Wwrite-strings
# -pedantic \
# -pedantic-errors \

OBJ = \
	  sai_extra_acl.o \
	  sai_extra_buffer.o \
	  sai_extra_fdb.o \
	  sai_extra_hash.o \
	  sai_extra_hostintf.o \
	  sai_extra_lag.o \
	  sai_extra_mirror.o \
	  sai_extra_neighbor.o \
	  sai_extra_nexthopgroup.o \
	  sai_extra_nexthop.o \
	  sai_extra_policer.o \
	  sai_extra_port.o \
	  sai_extra_qosmaps.o \
	  sai_extra_queue.o \
	  sai_extra_route.o \
	  sai_extra_routerintf.o \
	  sai_extra_router.o \
	  sai_extra_samplepacket.o \
	  sai_extra_schedulergroup.o \
	  sai_extra_scheduler.o \
	  sai_extra_stp.o \
	  sai_extra_switch.o \
	  sai_extra_tunnel.o \
	  sai_extra_udf.o \
	  sai_extra_vlan.o \
	  sai_extra_wred.o \
	  saiserialize.o \
	  sai_meta.o \
	  sai_meta_sanity.o \
	  sai_meta_acl.o \
	  sai_meta_buffer.o \
	  sai_meta_fdb.o \
	  sai_meta_hash.o \
	  sai_meta_hostintf.o \
	  sai_meta_lag.o \
	  sai_meta_mirror.o \
	  sai_meta_neighbor.o \
	  sai_meta_nexthopgroup.o \
	  sai_meta_nexthop.o \
	  sai_meta_policer.o \
	  sai_meta_port.o \
	  sai_meta_qosmaps.o \
	  sai_meta_queue.o \
	  sai_meta_route.o \
	  sai_meta_routerintf.o \
	  sai_meta_router.o \
	  sai_meta_samplepacket.o \
	  sai_meta_schedulergroup.o \
	  sai_meta_scheduler.o \
	  sai_meta_stp.o \
	  sai_meta_switch.o \
	  sai_meta_tunnel.o \
	  sai_meta_udf.o \
	  sai_meta_vlan.o \
	  sai_meta_wred.o

DEPS = sai_meta.h sai_extra.h

CFLAGS=-std=c++11 -I/usr/include/swss -I/usr/include/sai -I/usr/include -I. -I.. $(warnings)
LDFLAGS=-L/usr/lib -lswsscommon

all: tests

%.o: %.cpp $(DEPS)
	g++ -c -o $@ $< $(CFLAGS)

tests: tests.o $(OBJ) 
	g++ -o $@ $^ $(LDFLAGS)

.PHONY: clean

clean:
	rm -f *~ .*~ tests *.o *.lo

