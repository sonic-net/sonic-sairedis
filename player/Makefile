warnings = \
				-Wall \
				-Wcast-align \
				-Wcast-qual \
				-Wconversion \
				-Wdisabled-optimization \
				-Werror \
				-Wextra \
				-Wextra \
				-Wfloat-equal \
				-Wformat=2 \
				-Wformat-nonliteral \
				-Wformat-security \
				-Wformat-y2k \
				-Wimport \
				-Winit-self \
				-Winline \
				-Winvalid-pch \
				-Wlong-long \
				-Wmissing-field-initializers \
				-Wmissing-format-attribute \
				-Wmissing-include-dirs \
				-Wno-aggregate-return \
				-Wno-padded \
				-Wno-switch-enum \
				-Wno-unused-parameter \
				-Wno-unused-variable \
				-Wpacked \
				-Wpointer-arith \
				-Wredundant-decls \
				-Wshadow \
				-Wstack-protector \
				-Wstrict-aliasing=2 \
				-Wswitch \
				-Wswitch-default \
				-Wunreachable-code \
				-Wunused \
				-Wvariadic-macros \
				-Wno-write-strings

OBJ = \
	  player.o

DEPS = 

CFLAGS=-std=c++11 -I/usr/include/swss -I/usr/include/sai -I/usr/include -I.. $(warnings)
LDFLAGS=-lpthread -lsairedis -lswsscommon 

all: player

%.o: %.cpp $(DEPS)
	g++ -c -o $@ $< $(CFLAGS)

player: player.o $(OBJ)
	g++ -o $@ $^ $(LDFLAGS)

.PHONY: clean

clean:
	rm -f *~ .*~ player *.o *.lo

