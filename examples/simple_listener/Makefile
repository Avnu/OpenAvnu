OPT=-O2
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses

CC = gcc
INCFLAGS=-I../../daemons/mrpd -I../common
LDLIBS = -lpcap -lsndfile

all: simple_listener

simple_listener: simple_listener.o ../common/listener_mrp_client.o

simple_listener.o: simple_listener.c
	gcc -c $(INCFLAGS) $(CFLAGS) simple_listener.c

../common/listener_mrp_client.o:
	make -C ../common/ listener_mrp_client.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` simple_listener
	rm -f ../common/listener_mrp_client.o
