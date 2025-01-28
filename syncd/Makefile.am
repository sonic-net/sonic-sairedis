SUBDIRS = . tests

AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/lib -I$(top_srcdir)/vslib

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/.libs -lsaivs
else
SAILIB=-lsai
endif

bin_PROGRAMS = syncd syncd_request_shutdown syncd_tests

lib_LTLIBRARIES = libMdioIpcClient.la

noinst_LIBRARIES = libSyncd.a libSyncdRequestShutdown.a libMdioIpcClient.a

libSyncd_a_SOURCES = \
				AsicOperation.cpp \
				AsicView.cpp \
				BestCandidateFinder.cpp \
				BreakConfig.cpp \
				BreakConfigParser.cpp \
				CommandLineOptions.cpp \
				CommandLineOptionsParser.cpp \
				ComparisonLogic.cpp \
				FlexCounter.cpp \
				FlexCounterManager.cpp \
				GlobalSwitchId.cpp \
				HardReiniter.cpp \
				MdioIpcServer.cpp \
				MetadataLogger.cpp \
				NotificationHandler.cpp \
				NotificationProcessor.cpp \
				NotificationQueue.cpp \
				PortMap.cpp \
				PortMapParser.cpp \
				PortStateChangeHandler.cpp \
				RedisClient.cpp \
				RedisNotificationProducer.cpp \
				RequestShutdownCommandLineOptions.cpp \
				SaiAttr.cpp \
				SaiDiscovery.cpp \
				SaiObj.cpp \
				SaiSwitch.cpp \
				SaiSwitchInterface.cpp \
				ServiceMethodTable.cpp \
				SingleReiniter.cpp \
				SwitchNotifications.cpp \
				Syncd.cpp \
				TimerWatchdog.cpp \
				VendorSai.cpp \
				VidManager.cpp \
				VidManager.cpp \
				VirtualOidTranslator.cpp \
				WarmRestartTable.cpp \
				WatchdogScope.cpp \
				Workaround.cpp \
				ZeroMQNotificationProducer.cpp \
				syncd_main.cpp

libSyncd_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSyncd_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

syncd_SOURCES = main.cpp
if ASAN_ENABLED
syncd_SOURCES += Asan.cpp
libSyncd_a_CXXFLAGS += -DASAN_ENABLED
endif
syncd_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
syncd_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS) $(CFLAGS_ASAN)
syncd_LDADD = libSyncd.a $(top_srcdir)/lib/libSaiRedis.a -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
			  -ldl -lhiredis -lswsscommon $(SAILIB) -lpthread -lzmq $(CODE_COVERAGE_LIBS) $(EXTRA_LIBSAI_LDFLAGS)
syncd_LDFLAGS = $(LDFLAGS_ASAN) -rdynamic

if SAITHRIFT
libSyncd_a_CXXFLAGS += -DSAITHRIFT=yes
syncd_CXXFLAGS += -DSAITHRIFT=yes
syncd_LDADD += -lrpcserver -lthrift
endif

if SONIC_ASIC_PLATFORM_BROADCOM
libSyncd_a_CXXFLAGS += -DMDIO_ACCESS_USE_NPU
endif

if SONIC_ASIC_PLATFORM_MELLANOX
libSyncd_a_CPPFLAGS += -DSKIP_SAI_PORT_DISCOVERY
endif

libSyncdRequestShutdown_a_SOURCES = \
									RequestShutdown.cpp \
									RequestShutdownCommandLineOptions.cpp \
									RequestShutdownCommandLineOptionsParser.cpp

libSyncdRequestShutdown_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSyncdRequestShutdown_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

syncd_request_shutdown_SOURCES = syncd_request_shutdown.cpp
syncd_request_shutdown_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
syncd_request_shutdown_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
syncd_request_shutdown_LDADD = libSyncdRequestShutdown.a $(top_srcdir)/lib/libSaiRedis.a -lhiredis -lswsscommon -lpthread $(CODE_COVERAGE_LIBS)

libMdioIpcClient_a_SOURCES = MdioIpcClient.cpp

libMdioIpcClient_la_SOURCES = MdioIpcClient.cpp

libMdioIpcClient_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS) -Wno-format-truncation
libMdioIpcClient_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

libMdioIpcClient_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS) -Wno-format-truncation
libMdioIpcClient_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libMdioIpcClient_la_LIBADD = -lswsscommon $(CODE_COVERAGE_LIBS)

syncd_tests_SOURCES = tests.cpp
syncd_tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
syncd_tests_LDFLAGS = -Wl,-rpath,$(top_srcdir)/lib/.libs -Wl,-rpath,$(top_srcdir)/meta/.libs
syncd_tests_LDADD = libSyncd.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/lib/.libs -lsairedis \
			  -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = syncd_tests

if SAIVS
if DASHSAI
bin_PROGRAMS += syncd_dash
syncd_dash_SOURCES = $(syncd_SOURCES)
syncd_dash_CPPFLAGS = $(syncd_CPPFLAGS)
syncd_dash_CXXFLAGS = $(syncd_CXXFLAGS)
syncd_dash_LDADD = libSyncd.a $(top_srcdir)/lib/libSaiRedis.a -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
			  -ldl -lhiredis -lswsscommon -lsai -lprotobuf -lpiprotobuf -lpiprotogrpc -lgrpc++ -lpthread -lzmq $(CODE_COVERAGE_LIBS) $(EXTRA_LIBSAI_LDFLAGS)
syncd_dash_LDFLAGS = $(syncd_LDFLAGS)
endif
endif
