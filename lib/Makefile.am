AM_CXXFLAGS = $(OTAIINC)

#lib_LTLIBRARIES = libotairedis.la

noinst_LIBRARIES = libOtaiRedis.a

libOtaiRedis_a_SOURCES = \
						 Channel.cpp \
						 ClientConfig.cpp \
						 ClientOtai.cpp \
						 ClientServerOtai.cpp \
						 Context.cpp \
						 ContextConfig.cpp \
						 ContextConfigContainer.cpp \
						 Recorder.cpp \
						 RedisChannel.cpp \
						 RedisRemoteOtaiInterface.cpp \
						 RedisVidIndexGenerator.cpp \
						 Otai.cpp \
						 ServerConfig.cpp \
						 ServerOtai.cpp \
						 SkipRecordAttrContainer.cpp \
						 Switch.cpp \
						 SwitchConfig.cpp \
						 SwitchConfigContainer.cpp \
						 SwitchContainer.cpp \
						 Utils.cpp \
						 VirtualObjectIdManager.cpp \
						 ZeroMQChannel.cpp

BUILT_SOURCES = otai_redis.cpp

otai_redis.cpp: ../stub.pl $(top_srcdir)/OTAI/meta/otaimetadata.c
	../stub.pl -d ../OTAI/ -c ClientServerOtai -n otairedis -f otai_redis.cpp -s stub

clean-local:
	rm -f otai_redis.cpp

#libotairedis_la_SOURCES = otai_redis.cpp

libOtaiRedis_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libOtaiRedis_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

#libotairedis_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
#libotairedis_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
#libotairedis_la_LIBADD = -lhiredis -lswsscommon libOtaiRedis.a $(CODE_COVERAGE_LIBS)

#bin_PROGRAMS = tests

#tests_SOURCES = tests.cpp
#tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
#tests_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lotaimetadata -lotaimeta libotairedis.la -lzmq
#
#TESTS = tests
