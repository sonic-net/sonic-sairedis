AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/saisdkdump

bin_PROGRAMS = saisdkdump

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/.libs -lsaivs $(VPP_LIBS)
else
SAILIB=-lsai
endif

noinst_LIBRARIES = libsaisdkdump_profile.a

libsaisdkdump_profile_a_SOURCES = ProfileMap.cpp
libsaisdkdump_profile_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaisdkdump_profile_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

saisdkdump_SOURCES = saisdkdump.cpp
saisdkdump_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saisdkdump_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saisdkdump_LDADD = libsaisdkdump_profile.a -lhiredis -lswsscommon $(SAILIB) -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS) $(EXTRA_LIBSAI_LDFLAGS)
