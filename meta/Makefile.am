AM_CFLAGS = $(OTAIINC)
AM_CXXFLAGS = $(OTAIINC) -I$(top_srcdir)/lib

lib_LTLIBRARIES = libotaimetadata.la libotaimeta.la

BUILT_SOURCES = $(top_srcdir)/OTAI/meta/otaimetadata.c $(top_srcdir)/OTAI/meta/otaimetadata.h

$(top_srcdir)/OTAI/meta/otaimetadata.c $(top_srcdir)/OTAI/meta/otaimetadata.h: otaimeta-gen

otaimeta-gen:
	$(MAKE) -C $(top_srcdir)/OTAI/meta otaimetadata.c

clean-local:
	$(MAKE) -C $(top_srcdir)/OTAI/meta clean

libotaimetadata_la_SOURCES =  \
				$(top_srcdir)/OTAI/meta/otaimetadata.c \
				$(top_srcdir)/OTAI/meta/otaimetadatautils.c \
				$(top_srcdir)/OTAI/meta/otaiserialize.c

libotaimetadata_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libotaimetadata_la_CFLAGS = $(DBGFLAGS) $(AM_CFLAGS) -ansi $(CODE_COVERAGE_CFLAGS)

libotaimeta_la_SOURCES = \
				AttrKeyMap.cpp \
				Globals.cpp \
				Meta.cpp \
				MetaKeyHasher.cpp \
				Notification.cpp \
				NotificationFactory.cpp \
				NumberOidIndexGenerator.cpp \
				OidRefCounter.cpp \
				PerformanceIntervalTimer.cpp \
				PortRelatedSet.cpp \
				RedisSelectableChannel.cpp \
				OtaiAttrWrapper.cpp \
				OtaiAttributeList.cpp \
				OtaiInterface.cpp \
				OtaiObject.cpp \
				OtaiObjectCollection.cpp \
				OtaiSerialize.cpp \
				SelectableChannel.cpp \
				DummyOtaiInterface.cpp \
				ZeroMQSelectableChannel.cpp

libotaimeta_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libotaimeta_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libotaimeta_la_LIBADD = -lhiredis -lswsscommon libotaimetadata.la $(CODE_COVERAGE_LIBS)
