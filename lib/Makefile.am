AM_CXXFLAGS = $(SAIINC) -I$(top_srcdir)/stub

lib_LTLIBRARIES = libsairedis.la

noinst_LIBRARIES = libSaiRedis.a

libSaiRedis_a_SOURCES = \
						 Channel.cpp \
						 ClientConfig.cpp \
						 ClientSai.cpp \
						 ClientServerSai.cpp \
						 Context.cpp \
						 ContextConfig.cpp \
						 ContextConfigContainer.cpp \
						 Recorder.cpp \
						 RedisChannel.cpp \
						 RedisRemoteSaiInterface.cpp \
						 RedisVidIndexGenerator.cpp \
						 Sai.cpp \
						 ServerConfig.cpp \
						 ServerSai.cpp \
						 SkipRecordAttrContainer.cpp \
						 Switch.cpp \
						 SwitchConfig.cpp \
						 SwitchConfigContainer.cpp \
						 SwitchContainer.cpp \
						 Utils.cpp \
						 VirtualObjectIdManager.cpp \
						 ZeroMQChannel.cpp

libsairedis_la_SOURCES = sai_redis.cpp

libSaiRedis_a_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libSaiRedis_a_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)

libsairedis_la_CPPFLAGS = $(CODE_COVERAGE_CPPFLAGS)
libsairedis_la_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) $(CODE_COVERAGE_CXXFLAGS)
libsairedis_la_LIBADD = -lhiredis -lswsscommon libSaiRedis.a ../stub/libsaistub.a $(CODE_COVERAGE_LIBS)

bin_PROGRAMS = tests

tests_SOURCES = tests.cpp
tests_CXXFLAGS = $(DBGFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread -L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta libsairedis.la -lzmq

TESTS = tests
