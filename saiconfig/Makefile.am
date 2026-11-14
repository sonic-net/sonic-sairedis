AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib -I$(top_srcdir)/SAI/meta

bin_PROGRAMS = saiconfig

saiconfig_SOURCES = main.cpp SaiConfig.cpp
saiconfig_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
saiconfig_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
saiconfig_LDADD = -L$(top_srcdir)/meta/.libs -L$(top_srcdir)/lib/.libs \
				 -lsairedis -lsaimetadata -lsaimeta -lhiredis -lswsscommon -lpthread -lzmq $(CODE_COVERAGE_LIBS)

noinst_LIBRARIES = libsaiconfig.a

libsaiconfig_a_SOURCES = SaiConfig.cpp
libsaiconfig_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaiconfig_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
