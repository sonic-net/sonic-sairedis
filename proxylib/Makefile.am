AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib -I$(top_srcdir)/meta

lib_LTLIBRARIES = libsaiproxy.la

noinst_LIBRARIES = libSaiProxy.a

libSaiProxy_a_SOURCES = \
						 Options.cpp \
						 Proxy.cpp \
						 Sai.cpp

BUILT_SOURCES = sai_proxy.cpp

sai_proxy.cpp: $(top_srcdir)/SAI/meta/saimetadata.c ../stub.pl
	../stub.pl -d ../SAI/ -c Sai -n saiproxy -f sai_proxy.cpp -s stub

clean-local:
	rm -f sai_proxy.cpp

libsaiproxy_la_SOURCES = sai_proxy.cpp

libSaiProxy_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiProxy_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

libsaiproxy_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaiproxy_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsaiproxy_la_LIBADD = -lhiredis -lswsscommon libSaiProxy.a $(CODE_COVERAGE_LIBS)

# TODO - lib sai proxy server that will accept VendorSai

