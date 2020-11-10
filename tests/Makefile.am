AM_CPPFLAGS = -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

bin_PROGRAMS = vssyncd tests

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

SAILIB=-L$(top_srcdir)/vslib/src/.libs -lsaivs

vssyncd_SOURCES = ../syncd/main.cpp

vssyncd_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS)
vssyncd_LDADD = ../syncd/libSyncd.a ../lib/src/libSaiRedis.a -lhiredis -lswsscommon $(SAILIB) -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -ldl -lzmq

if SAITHRIFT
vssyncd_LDADD += -lrpcserver -lthrift
endif

tests_SOURCES = tests.cpp
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread \
			  $(top_srcdir)/lib/src/libsairedis.la \
			  $(top_srcdir)/syncd/libSyncd.a \
			  -L$(top_srcdir)/meta/.libs \
			  -lsaimetadata -lsaimeta -lzmq

TESTS = aspellcheck.pl conflictnames.pl swsslogentercheck.sh tests BCM56850.pl MLNX2700.pl
