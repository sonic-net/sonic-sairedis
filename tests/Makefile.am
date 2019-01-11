AM_CPPFLAGS = -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/lib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

bin_PROGRAMS = vssyncd

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

SAILIB=-L$(top_srcdir)/vslib/src/.libs -lsaivs

vssyncd_SOURCES = \
				../syncd/main.cpp \
				../syncd/syncd.cpp \
				../syncd/syncd_saiswitch.cpp \
				../syncd/syncd_hard_reinit.cpp \
				../syncd/syncd_notifications.cpp \
				../syncd/syncd_applyview.cpp \
				../syncd/syncd_flex_counter.cpp

vssyncd_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS)
vssyncd_LDADD = -lhiredis -lswsscommon $(SAILIB) -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -ldl

TESTS = aspellcheck.pl mlnx.pl brcm.pl swsslogentercheck.sh
