OPT=-O2
CFLAGS=$(OPT) -Wall -W -Wno-parentheses

CC=gcc
INCFLAGS=-I../../lib/igb
LDLIBS=-ligb -lpci -lz -lrt -lm -pthread
LDFLAGS=-L../../lib/igb

all: avb

avb:
	gcc $(INCFLAGS) -c avb.c

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` avb.o

