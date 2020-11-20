# since we have SAI sumbodule we will use submodule headers
AM_CPPFLAGS = -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental -I$(top_srcdir)/lib/inc

if DEBUG
DBGFLAGS = -ggdb -D_DEBUG_
else
DBGFLAGS = -g
endif

lib_LTLIBRARIES = libsaimeta.la libsaimetadata.la

WARN = \
	-ansi \
	-Wall \
	-Wcast-align \
	-Wcast-qual \
	-Wconversion \
	-Wdisabled-optimization \
	-Werror \
	-Wextra \
	-Wextra \
	-Wfloat-equal \
	-Wformat=2 \
	-Wformat-nonliteral \
	-Wformat-security \
	-Wformat-y2k \
	-Wimport \
	-Winit-self \
	-Winline \
	-Winvalid-pch \
	-Wmissing-field-initializers \
	-Wmissing-format-attribute \
	-Wmissing-include-dirs \
	-Wmissing-noreturn \
	-Wno-aggregate-return \
	-Wno-padded \
	-Wno-switch-enum \
	-Wno-unused-parameter \
	-Wpacked \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wshadow \
	-Wstack-protector \
	-Wstrict-aliasing=3 \
	-Wswitch \
	-Wswitch-default \
	-Wunreachable-code \
	-Wunused \
	-Wvariadic-macros \
	-Wwrite-strings

# we are not adding CPPFLAGS since they are added
# automatic to each C preprocessor, and currently
# we add -std=c++11 flag to CFLAGS_COMMON which
# will cause compilaton error on C files
# and we already confirmed that SAI/meta files
# can be compiled with success
#
# NOTE: this compilation could be moved to configure.ac

libsaimeta_la_SOURCES =  \
					  $(top_srcdir)/SAI/meta/saimetadata.c \
					  $(top_srcdir)/SAI/meta/saimetadatautils.c \
					  $(top_srcdir)/SAI/meta/saiserialize.c

libsaimeta_la_CFLAGS = $(DBGFLAGS) $(WARN)

$(top_srcdir)/SAI/meta/saimetadata.h $(top_srcdir)/SAI/meta/saimetadata.c: saimeta-gen

saimeta-gen:
	make -C $(top_srcdir)/SAI/meta

clean-local:
	make -C $(top_srcdir)/SAI/meta clean

libsaimetadata_la_SOURCES = \
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


BUILT_SOURCES = $(top_srcdir)/SAI/meta/saimetadata.h $(top_srcdir)/SAI/meta/saimetadata.c

libsaimetadata_la_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
libsaimetadata_la_LIBADD = -lhiredis -lswsscommon libsaimeta.la

bin_PROGRAMS = tests

tests_SOURCES = \
				tests.cpp \
				DummySaiInterface.cpp \
				MetaTestSaiInterface.cpp \
				NumberOidIndexGenerator.cpp \
				../lib/src/VirtualObjectIdManager.cpp \
				../lib/src/SwitchConfig.cpp \
				../lib/src/SwitchConfigContainer.cpp

tests_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread libsaimetadata.la libsaimeta.la

TESTS = tests
