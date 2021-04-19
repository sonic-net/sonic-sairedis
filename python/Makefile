
all: sairedis.cpp setup.py
	python setup.py build

install: all
	python setup.py install --user

check:
	true

.PHONY: clean distclean

clean:
	rm -f *.o *~
	rm -rf build

distclean:
	rm -f *.o *~
	rm -rf build
