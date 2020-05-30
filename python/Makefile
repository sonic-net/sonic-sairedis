
all: sairedis.cpp setup.py
	python setup.py build

install: all
	python setup.py install --user

.PHONY: clean

clean:
	rm -f *.o *~
	rm -rf build

