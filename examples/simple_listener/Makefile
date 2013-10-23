CC = gcc
LDLIBS = -std=gnu99 -lpcap -lsndfile

simple_listener: simple_listener.c
	$(CC) -o $@ $< $(LDLIBS) $(LDFLAGS)

all: simple_listener

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` simple_listener
