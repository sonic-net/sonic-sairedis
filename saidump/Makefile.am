AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib

bin_PROGRAMS = saidump

saidump_SOURCES = main.cpp SaiDump.cpp
saidump_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saidump_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saidump_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
				-L$(top_srcdir)/lib/.libs -lsairedis -lzmq $(CODE_COVERAGE_LIBS)

noinst_LIBRARIES = libsaidump.a
libsaidump_a_SOURCES = SaiDump.cpp
libsaidump_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaidump_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
