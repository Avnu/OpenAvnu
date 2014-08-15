CC = gcc
LDLIBS = -std=gnu99 -lpcap -lsndfile -pthread -ljack

all: jack_listener.c
	$(CC) -o jack_listener jack_listener.c $(LDLIBS) $(LDFLAGS)

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` jack_listener
