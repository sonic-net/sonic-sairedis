
all: _pysairedis.so

INCLUDE=-I../SAI/inc -I../SAI/meta -I../SAI/experimental -I../lib/inc -I../

pysairedis_wrap.cpp: pysairedis.i pysairedis.h pysairedis.cpp *.h
	swig -Wall -c++ -python -keyword -DSWIGWORDSIZE64 $(INCLUDE) -I. -o pysairedis_wrap.cpp pysairedis.i

top_srcdir=../

_pysairedis.so: pysairedis_wrap.cpp pysairedis.cpp
	g++ -fPIC -c pysairedis_wrap.cpp pysairedis.cpp -std=c++11 $(INCLUDE) -I../../common -I/usr/include/python2.7 -shared -fPIC
	g++ -shared \
		-lhiredis -lswsscommon -lpthread \
		-L$(top_srcdir)/lib/src/.libs -lsairedis \
		-L$(top_srcdir)/meta/.libs -lsaimetadata -lsaimeta \
		-lzmq \
		pysairedis.o pysairedis_wrap.o -o _pysairedis.so

.PHONY: clean

clean:
	rm -f pysairedis_wrap.cpp _pysairedis.so pysairedis.py *.o *.so .*swp .*swo *~ .*~ sairedis.rec *.pyc
	
