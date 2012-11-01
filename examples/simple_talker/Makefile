OPT=-O2
CFLAGS=$(OPT) -Wall -W -Wno-parentheses

CC=gcc
INCFLAGS=-I../../lib/igb
LDLIBS=-ligb -lpci -lz -lrt -lm -pthread
LDFLAGS=-L../../lib/igb

all: simple_talker

simple_talker: simple_talker.o

simple_talker.o: simple_talker.c
	gcc -c $(INCFLAGS) -I../../daemons/mrpd $(CFLAGS) simple_talker.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` simple_talker

