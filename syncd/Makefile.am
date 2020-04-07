AM_CPPFLAGS = -I$(top_srcdir)/lib/inc -I$(top_srcdir)/vslib/inc -I$(top_srcdir)/SAI/inc -I$(top_srcdir)/SAI/meta -I$(top_srcdir)/SAI/experimental

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

noinst_LIBRARIES = libsyncd.a
libsyncd_a_SOURCES = \
				Syncd.cpp \
				RedisClient.cpp \
				WarmRestartTable.cpp \
				RequestShutdownCommandLineOptions.cpp \
				GlobalSwitchId.cpp \
				MetadataLogger.cpp \
				Workaround.cpp \
				ServiceMethodTable.cpp \
				SwitchNotifications.cpp \
				ComparisonLogic.cpp \
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
				../lib/src/SwitchConfig.cpp \
				../lib/src/SwitchConfigContainer.cpp \
				../lib/src/VirtualObjectIdManager.cpp \
				../lib/src/RedisVidIndexGenerator.cpp \
				../lib/src/ContextConfig.cpp \
				../lib/src/ContextConfigContainer.cpp \
				VidManager.cpp \
				VidManager.cpp \
				AsicOperation.cpp \
				AsicView.cpp \
				SaiObj.cpp \
				SaiAttr.cpp \
				VendorSai.cpp \
				syncd.cpp \
				TimerWatchdog.cpp \
				NotificationQueue.cpp \
				CommandLineOptions.cpp \
				CommandLineOptionsParser.cpp \
				PortMap.cpp \
				PortMapParser.cpp

libsyncd_a_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS) -std=c++14

syncd_SOURCES = main.cpp

syncd_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON) $(SAIFLAGS)
syncd_LDADD = -L$(top_srcdir)/meta/.libs libsyncd.a -lsaimetadata -lsaimeta -ldl -lhiredis -lswsscommon $(SAILIB) -lpthread

if SAITHRIFT
libsyncd_a_CPPFLAGS += -DSAITHRIFT=yes
syncd_CPPFLAGS += -DSAITHRIFT=yes
syncd_LDADD += -lrpcserver -lthrift
endif

if sonic_asic_platform_mellanox
syncd_CPPFLAGS += -DSAI_SUPPORT_UNINIT_DATA_PLANE_ON_REMOVAL
libsyncd_a_CPPFLAGS += -DSAI_SUPPORT_UNINIT_DATA_PLANE_ON_REMOVAL
endif

syncd_request_shutdown_SOURCES = \
								 RequestShutdown.cpp \
								 RequestShutdownCommandLineOptions.cpp \
								 RequestShutdownCommandLineOptionsParser.cpp \
								 ../lib/src/SwitchConfig.cpp \
								 ../lib/src/SwitchConfigContainer.cpp \
								 ../lib/src/ContextConfig.cpp \
								 ../lib/src/ContextConfigContainer.cpp \
								 syncd_request_shutdown.cpp

syncd_request_shutdown_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
syncd_request_shutdown_LDADD = -lhiredis -lswsscommon -lpthread

tests_SOURCES = tests.cpp

tests_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
tests_LDADD = libsyncd.a -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/lib/src/.libs -lsairedis -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta
TESTS = tests
