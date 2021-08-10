AM_CXXFLAGS = -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/experimental -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/lib/inc

bin_PROGRAMS = saidump

saidump_SOURCES = saidump.cpp
saidump_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saidump_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saidump_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -L$(top_srcdir)/lib/src/.libs -lsairedis -lzmq $(CODE_COVERAGE_LIBS)
