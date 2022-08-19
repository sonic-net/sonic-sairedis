AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/syncd/sdk_source_topflite/include

bin_PROGRAMS = saisdkdump

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/.libs -lsaivs
else
SAILIB=-lsai
endif

saisdkdump_SOURCES = saisdkdump.cpp
saisdkdump_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saisdkdump_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saisdkdump_LDADD = -lhiredis -lswsscommon $(SAILIB) -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS) -L$(top_srcdir)/syncd -lsaitry
