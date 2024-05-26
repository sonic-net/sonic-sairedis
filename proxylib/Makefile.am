AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/stub

lib_LTLIBRARIES = libsaiproxy.la

noinst_LIBRARIES = libSaiProxy.a

libSaiProxy_a_SOURCES = \
						 Sai.cpp

libsaiproxy_la_SOURCES = sai_proxy.cpp

libSaiProxy_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiProxy_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

libsaiproxy_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaiproxy_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsaiproxy_la_LIBADD = -lhiredis -lswsscommon libSaiProxy.a ../stub/libsaistub.a $(CODE_COVERAGE_LIBS)

# TODO - lib sai proxy server that will accept VendorSai

