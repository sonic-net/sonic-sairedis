AM_CXXFLAGS = -I$(top_srcdir)/lib/inc -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

bin_PROGRAMS = syncd syncd_request_shutdown tests

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

if SAIVS
SAILIB=-L$(top_srcdir)/vslib/src/.libs -lsaivs
else
SAILIB=-lsai
endif

noinst_LIBRARIES = libSyncd.a libSyncdRequestShutdown.a
libSyncd_a_SOURCES = \
				WatchdogScope.cpp \
				SaiSwitchInterface.cpp \
				ZeroMQSelectableChannel.cpp \
				RedisSelectableChannel.cpp \
				SelectableChannel.cpp \
				ZeroMQNotificationProducer.cpp \
				RedisNotificationProducer.cpp \
				ComparisonLogic.cpp \
				Syncd.cpp \
				BreakConfig.cpp \
				BreakConfigParser.cpp \
				RedisClient.cpp \
				WarmRestartTable.cpp \
				RequestShutdownCommandLineOptions.cpp \
				GlobalSwitchId.cpp \
				MetadataLogger.cpp \
				Workaround.cpp \
				ServiceMethodTable.cpp \
				SwitchNotifications.cpp \
				VirtualOidTranslator.cpp \
				NotificationProcessor.cpp \
				NotificationHandler.cpp \
				SingleReiniter.cpp \
				HardReiniter.cpp \
				SaiDiscovery.cpp \
				SaiSwitch.cpp \
				BestCandidateFinder.cpp \
				FlexCounterManager.cpp \
				FlexCounter.cpp \
				VidManager.cpp \
				VidManager.cpp \
				AsicOperation.cpp \
				AsicView.cpp \
				SaiObj.cpp \
				SaiAttr.cpp \
				VendorSai.cpp \
				syncd_main.cpp \
				TimerWatchdog.cpp \
				NotificationQueue.cpp \
				CommandLineOptions.cpp \
				CommandLineOptionsParser.cpp \
				PortMap.cpp \
				PortMapParser.cpp

libSyncd_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSyncd_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

syncd_SOURCES = main.cpp
syncd_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
syncd_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
syncd_LDADD = libSyncd.a ../lib/src/libSaiRedis.a -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -ldl -lhiredis -lswsscommon $(SAILIB) -lpthread -lzmq $(CODE_COVERAGE_LIBS)

if SAITHRIFT
libSyncd_a_CXXFLAGS += -DSAITHRIFT=yes
syncd_CXXFLAGS += -DSAITHRIFT=yes
syncd_LDADD += -lrpcserver -lthrift
endif

if sonic_asic_platform_mellanox
syncd_CXXFLAGS += -DSAI_SUXXORT_UNINIT_DATA_PLANE_ON_REMOVAL
libSyncd_a_CXXFLAGS += -DSAI_SUXXORT_UNINIT_DATA_PLANE_ON_REMOVAL
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
syncd_request_shutdown_LDADD = libSyncdRequestShutdown.a ../lib/src/libSaiRedis.a -lhiredis -lswsscommon -lpthread $(CODE_COVERAGE_LIBS)

tests_SOURCES = tests.cpp

tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = libSyncd.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/lib/src/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta -lzmq $(CODE_COVERAGE_LIBS)

TESTS = tests
