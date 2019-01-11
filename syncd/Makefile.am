AM_CPPFLAGS = -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

bin_PROGRAMS = syncd syncd_request_shutdown tests

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/src/.libs -lsaivs
else
if sonic_asic_platform_barefoot
AM_CPPFLAGS += -I/opt/bfn/install/include
AM_LDFLAGS = -Wl,-unresolved-symbols=ignore-in-shared-libs
SAILIB=-L/opt/bfn/install/lib -lswitchsai
else
SAILIB=-lsai
endif
endif

syncd_SOURCES = \
				main.cpp \
				syncd.cpp \
				syncd_saiswitch.cpp \
				syncd_hard_reinit.cpp \
				syncd_notifications.cpp \
				syncd_applyview.cpp \
				syncd_flex_counter.cpp

syncd_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS)
syncd_LDADD = -lhiredis -lswsscommon $(SAILIB) -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -ldl

if SAITHRIFT
syncd_CPPFLAGS += -DSAITHRIFT=yes
syncd_LDADD += -lrpcserver -lthrift
endif

if sonic_asic_platform_mellanox
syncd_CPPFLAGS += -DSAI_SUPPORT_UNINIT_DATA_PLANE_ON_REMOVAL
endif

syncd_request_shutdown_SOURCES = syncd_request_shutdown.cpp
syncd_request_shutdown_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
syncd_request_shutdown_LDADD = -lhiredis -lswsscommon -lpthread

tests_SOURCES = \
				tests.cpp \
				syncd.cpp \
				syncd_saiswitch.cpp \
				syncd_hard_reinit.cpp \
				syncd_notifications.cpp \
				syncd_applyview.cpp \
				syncd_flex_counter.cpp

tests_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/lib/src/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta

if RTEST
TESTS = tests
endif
