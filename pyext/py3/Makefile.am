SWIG_SRC=../pysairedis.i
SOURCES=../pysairedis.h ../pysairedis.cpp

pkgpython3_PYTHON = pysairedis.py __init__.py
pkgpy3exec_LTLIBRARIES = _pysairedis.la

INCLUDE=-I../../SAI/inc -I../../SAI/meta -I../../SAI/experimental -I../../lib/inc -I..

BUILT_SOURCES = pysairedis_wrap.cpp

_pysairedis_la_SOURCES = pysairedis_wrap.cpp $(SOURCES)
_pysairedis_la_CXXFLAGS = $(INCLUDE) -I/usr/include/python$(PYTHON3_VERSION) $(AM_CXXFLAGS) $(CXXFLAGS_COMMON) \
						  -Wno-cast-qual -Wno-shadow -Wno-redundant-decls -Wno-conversion $(NO_CAST_FUNCTION_TYPE)

_pysairedis_la_LDFLAGS = -module \
		-lhiredis -lswsscommon -lpthread \
		-L$(top_srcdir)/lib/src/.libs -lsairedis \
		-L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
		-lzmq

_pysairedis_la_LIBADD = $(PYTHON3_BLDLIBRARY)

SWIG_FLAG = -Wall -c++ -python -keyword
if ARCH64
SWIG_FLAG += -DSWIGWORDSIZE64
endif

pysairedis_wrap.cpp: $(SWIG_SRC) $(SOURCES)
	$(SWIG) $(SWIG_FLAG) $(INCLUDE) -o $@ $<

CLEANFILES = pysairedis_wrap.cpp
