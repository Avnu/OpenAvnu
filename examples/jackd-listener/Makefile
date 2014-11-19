CC = gcc
OPT=-O2 -g
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses -std=gnu99
INCFLAGS=-I../../daemons/mrpd -I../common
LDLIBS = -lpcap -lsndfile -pthread -ljack

all: jack_listener

jack_listener: jack_listener.o ../common/listener_mrp_client.o

jack_listener.o: jack_listener.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c jack_listener.c

../common/listener_mrp_client.o:
	make -C ../common/ listener_mrp_client.o

%: %.o
	$(CC) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` jack_listener
