AM_CPPFLAGS = -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

bin_PROGRAMS = saidiscovery

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/src/.libs -lsaivs
else
if sonic_asic_platform_barefoot
SAILIB=-L/opt/bfn/install/lib -lswitchsai -lswitchapi -lbf_switchd_lib -ltofinopdfixed_thrift -ldriver -lbfutils -lbfsys -lbfutils -L/opt/bfn/install/lib/tofinopd/switch -lpd -lpdcli -lpdthrift
else
SAILIB=-lsai
endif
endif

saidiscovery_SOURCES = saidiscovery.cpp

saidiscovery_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS)
saidiscovery_LDADD = -lhiredis -lswsscommon $(SAILIB) -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta
