# since we have SAI sumbodule we will use submodule headers
AM_CFLAGS = -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental
AM_CXXFLAGS = -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental -I$(top_srcdir)/lib/inc

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

lib_LTLIBRARIES = libsaimetadata.la libsaimeta.la

BUILT_SOURCES = $(top_srcdir)/SAI/meta/saimetadata.c $(top_srcdir)/SAI/meta/saimetadata.h

$(top_srcdir)/SAI/meta/saimetadata.c $(top_srcdir)/SAI/meta/saimetadata.h: saimeta-gen

saimeta-gen:
	make -C $(top_srcdir)/SAI/meta saimetadata.c

clean-local:
	make -C $(top_srcdir)/SAI/meta clean

libsaimetadata_la_SOURCES =  \
				$(top_srcdir)/SAI/meta/saimetadata.c \
				$(top_srcdir)/SAI/meta/saimetadatautils.c \
				$(top_srcdir)/SAI/meta/saiserialize.c

libsaimetadata_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaimetadata_la_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) -ansi $(CODE_COVERAGE_CFLAGS)


libsaimeta_la_SOURCES = \
				../lib/src/SaiInterface.cpp \
				SaiAttributeList.cpp \
				saiserialize.cpp \
				SaiAttrWrapper.cpp \
				OidRefCounter.cpp \
				AttrKeyMap.cpp \
				Globals.cpp \
				SaiObject.cpp \
				SaiObjectCollection.cpp \
				PortRelatedSet.cpp \
				MetaKeyHasher.cpp \
				Meta.cpp

libsaimeta_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsaimeta_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsaimeta_la_LIBADD = -lhiredis -lswsscommon libsaimetadata.la $(CODE_COVERAGE_LIBS)

bin_PROGRAMS = tests

tests_SOURCES = \
				tests.cpp \
				DummySaiInterface.cpp \
				MetaTestSaiInterface.cpp \
				NumberOidIndexGenerator.cpp \
				../lib/src/VirtualObjectIdManager.cpp \
				../lib/src/SwitchConfig.cpp \
				../lib/src/SwitchConfigContainer.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread libsaimetadata.la libsaimeta.la

TESTS = tests
