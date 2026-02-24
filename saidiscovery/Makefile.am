AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/meta -I$(top_srcdir)/lib

bin_PROGRAMS = saidiscovery

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/.libs -lsaivs $(VPP_LIBS)
else
SAILIB=-lsai
endif

saidiscovery_SOURCES = saidiscovery.cpp

saidiscovery_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saidiscovery_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saidiscovery_LDADD = -lhiredis -lswsscommon $(top_srcdir)/syncd/libSyncd.a $(SAILIB) -lpthread \
					 -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq -lz $(CODE_COVERAGE_LIBS) $(EXTRA_LIBSAI_LDFLAGS)
