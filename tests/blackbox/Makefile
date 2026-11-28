# Builds the mock SAI library and the load client of the syncd black-box
# benchmark against the sairedis tree this directory is in. Build the tree
# first (./autogen.sh && ./configure --with-sai=vs && make), then run make
# here. Outputs go to $(BUILD).

TOP ?= $(abspath ../..)
BUILD ?= build

CXX ?= g++
CXXFLAGS ?= -g -O2

# same warnings as the rest of sairedis (configure.ac CXXFLAGS_COMMON)
WARNINGS = -ansi -fPIC -pipe -std=c++14 -Wall -Wcast-align -Wcast-qual \
	-Wconversion -Wdisabled-optimization -Werror -Wextra -Wfloat-equal \
	-Wformat=2 -Wformat-nonliteral -Wformat-security -Wformat-y2k -Wimport \
	-Winit-self -Wno-inline -Winvalid-pch -Wmissing-field-initializers \
	-Wmissing-format-attribute -Wmissing-include-dirs -Wmissing-noreturn \
	-Wno-aggregate-return -Wno-padded -Wno-switch-enum -Wno-unused-parameter \
	-Wpacked -Wpointer-arith -Wredundant-decls -Wshadow -Wstack-protector \
	-Wstrict-aliasing=3 -Wswitch -Wunreachable-code -Wunused \
	-Wvariadic-macros -Wwrite-strings -Wno-switch-default -Wno-psabi

INCLUDES = -I$(TOP) -I$(TOP)/meta -I$(TOP)/lib -I$(TOP)/SAI/inc \
	-I$(TOP)/SAI/experimental -I$(TOP)/SAI/meta -Isaimock -I$(BUILD)

LIBDIRS = -L$(TOP)/meta/.libs -L$(TOP)/lib/.libs

MOCK_SOURCES = saimock/MockSai.cpp saimock/MockSwitch.cpp saimock/MockControl.cpp
MOCK_OBJECTS = $(MOCK_SOURCES:saimock/%.cpp=$(BUILD)/%.o) $(BUILD)/sai_stub.o

SAI_HEADERS = $(wildcard $(TOP)/SAI/inc/sai*.h $(TOP)/SAI/experimental/sai*.h)

# the headers each compilation read, as the SAI's and sairedis's, so that a
# changed one rebuilds what includes it
DEPFLAGS = -MMD -MP -MT $@ -MF $@.d

all: $(BUILD)/libsaimock.so $(BUILD)/saibench

$(BUILD):
	mkdir -p $(BUILD)

# API tables of every SAI API, forwarding to saimock::MockSai. stub.pl leaves
# the bulk stats globals unimplemented, as their names do not match
# SaiInterface's bulkGetStats and bulkClearStats: forward them too.
$(BUILD)/sai_stub.cpp: $(TOP)/stub.pl $(SAI_HEADERS) Makefile | $(BUILD)
	perl $(TOP)/stub.pl -d $(TOP)/SAI -c MockSai -n saimock -f $@.tmp -s stub > $(BUILD)/stub.log
	perl -0pi -e 's/SWSS_LOG_ERROR\("FIXME, no implementation for bulkObjectGetStats!"\);\s*return SAI_STATUS_NOT_IMPLEMENTED;/return stub->bulkGetStats(switch_id, object_type, object_count, object_key, number_of_counters, counter_ids, mode, object_statuses, counters);/; s/SWSS_LOG_ERROR\("FIXME, no implementation for bulkObjectClearStats!"\);\s*return SAI_STATUS_NOT_IMPLEMENTED;/return stub->bulkClearStats(switch_id, object_type, object_count, object_key, number_of_counters, counter_ids, mode, object_statuses);/' $@.tmp
	grep -q "stub->bulkGetStats" $@.tmp && grep -q "stub->bulkClearStats" $@.tmp
	mv $@.tmp $@

$(BUILD)/%.o: saimock/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(DEPFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD)/sai_stub.o: $(BUILD)/sai_stub.cpp
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(DEPFLAGS) $(INCLUDES) -c -o $@ $<

$(BUILD)/libsaimock.so: $(MOCK_OBJECTS)
	$(CXX) $(CXXFLAGS) -shared -Wl,-soname,libsaimock.so -o $@ $^ \
		$(LIBDIRS) -lsaimeta -lsaimetadata -lswsscommon -lpthread

$(BUILD)/saibench: saibench/saibench.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(WARNINGS) $(DEPFLAGS) $(INCLUDES) -o $@ $< \
		$(LIBDIRS) -lsairedis -lsaimeta -lsaimetadata -lswsscommon -lhiredis -lzmq -lpthread

clean:
	rm -rf $(BUILD)

.PHONY: all clean

-include $(wildcard $(BUILD)/*.d)
