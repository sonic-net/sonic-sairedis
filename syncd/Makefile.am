SUBDIRS = . tests

AM_CXXFLAGS = $(OTAIINC) -I$(top_srcdir)/lib -I$(top_srcdir)/vslib -I$(top_srcdir)/OTAI/meta/

if SAIVS
OTAILIB=-L$(top_srcdir)/vslib/.libs -lotaivs
else
OTAILIB=-lotai
endif

bin_PROGRAMS = syncd syncd_request_shutdown syncd_tests

lib_LTLIBRARIES = libMdioIpcClient.la

noinst_LIBRARIES = libSyncd.a libSyncdRequestShutdown.a libMdioIpcClient.a

libSyncd_a_SOURCES = \
				AsicOperation.cpp \
				CommandLineOptions.cpp \
				CommandLineOptionsParser.cpp \
				GlobalSwitchId.cpp \
				HardReiniter.cpp \
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
				OtaiAttr.cpp \
				OtaiDiscovery.cpp \
				OtaiObj.cpp \
				OtaiSwitch.cpp \
				OtaiSwitchInterface.cpp \
				ServiceMethodTable.cpp \
				SingleReiniter.cpp \
				SwitchNotifications.cpp \
				Syncd.cpp \
				TimerWatchdog.cpp \
				VendorOtai.cpp \
				VidManager.cpp \
				VidManager.cpp \
				VirtualOidTranslator.cpp \
				WarmRestartTable.cpp \
				WatchdogScope.cpp \
				Workaround.cpp \
				ZeroMQNotificationProducer.cpp \
				syncd_main.cpp

				AsicView.cpp \
				MdioIpcServer.cpp \
				BestCandidateFinder.cpp \
				BreakConfig.cpp \
				BreakConfigParser.cpp
				ComparisonLogic.cpp \
				FlexCounter.cpp \
				FlexCounterManager.cpp 

libSyncd_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSyncd_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

syncd_SOURCES = main.cpp
if ASAN_ENABLED
syncd_SOURCES += Asan.cpp
libSyncd_a_CXXFLAGS += -DASAN_ENABLED
endif
syncd_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
syncd_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS) $(CFLAGS_ASAN)
syncd_LDADD = libSyncd.a $(top_srcdir)/lib/libOtaiRedis.a -L$(top_srcdir)/meta/.libs -lotaimetadata -lotaimeta \
			  -ldl -lhiredis -lswsscommon $(OTAILIB) -lpthread -lzmq $(CODE_COVERAGE_LIBS) $(EXTRA_LIBOTAI_LDFLAGS)
syncd_LDFLAGS = $(LDFLAGS_ASAN) -rdynamic

#if OTAITHRIFT
#libSyncd_a_CXXFLAGS += -DOTAITHRIFT=yes
#syncd_CXXFLAGS += -DOTAITHRIFT=yes
#syncd_LDADD += -lrpcserver -lthrift
#endif

if SONIC_ASIC_PLATFORM_BROADCOM
libSyncd_a_CXXFLAGS += -DMDIO_ACCESS_USE_NPU
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
syncd_request_shutdown_LDADD = libSyncdRequestShutdown.a $(top_srcdir)/lib/libOtaiRedis.a -lhiredis -lswsscommon -lpthread $(CODE_COVERAGE_LIBS)

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
syncd_tests_LDADD = libSyncd.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/lib/.libs -lotairedis \
			  -L$(top_srcdir)/meta/.libs -lotaimetadata -lotaimeta -lzmq $(CODE_COVERAGE_LIBS)

#TESTS = syncd_tests
#
#if OTAIVS
#if DASHOTAI
#bin_PROGRAMS += syncd_dash
#syncd_dash_SOURCES = $(syncd_SOURCES)
#syncd_dash_CPPFLAGS = $(syncd_CPPFLAGS)
#syncd_dash_CXXFLAGS = $(syncd_CXXFLAGS)
#syncd_dash_LDADD = libSyncd.a $(top_srcdir)/lib/libOtaiRedis.a -L$(top_srcdir)/meta/.libs -lotaimetadata -lotaimeta \
#			  -ldl -lhiredis -lswsscommon -lotai -lprotobuf -lpiprotobuf -lpiprotogrpc -lgrpc++ -lpthread -lzmq $(CODE_COVERAGE_LIBS) $(EXTRA_LIBOTAI_LDFLAGS)
#syncd_dash_LDFLAGS = $(syncd_LDFLAGS)
#endif
#endif
