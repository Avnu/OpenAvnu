OPT=-O2
CFLAGS=$(OPT) -Wall -W -Wno-parentheses

CC=gcc
INCFLAGS=-I../../lib/igb
LDLIBS=-ligb -lpci -lz -lrt -lm -pthread
LDFLAGS=-L../../lib/igb

all: avb.o talker_mrp_client.o listener_mrp_client.o

avb.o:
	gcc $(INCFLAGS) -c avb.c

talker_mrp_client.o: talker_mrp_client.c talker_mrp_client.h
	gcc $(CFLAGS) -I../../daemons/mrpd -c talker_mrp_client.c

listener_mrp_client.o: listener_mrp_client.c listener_mrp_client.h
	gcc $(CFLAGS) -I../../daemons/mrpd -c listener_mrp_client.c

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` avb.o

