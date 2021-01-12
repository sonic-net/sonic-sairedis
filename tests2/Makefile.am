AM_CPPFLAGS = 

bin_PROGRAMS = tests

if DEBUG
DBGFLAGS = -ggdb -DDEBUG
else
DBGFLAGS = -g
endif

tests_SOURCES = tests.cpp
tests_CPPFLAGS = $(DBGFLAGS) $(AM_CPPFLAGS) $(CFLAGS_COMMON)
tests_LDADD = -lhiredis -lswsscommon -lpthread

TESTS = tests
