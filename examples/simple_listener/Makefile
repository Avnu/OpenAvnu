OPT=-O2
CFLAGS=$(OPT) -Wall -W -Wno-parentheses

CC = gcc
LDLIBS = -lpcap -lsndfile

all: simple_listener

simple_listener: simple_listener.o

simple_listener.o: simple_listener.c
	gcc -c $(INCFLAGS) $(CFLAGS) simple_listener.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` simple_listener
