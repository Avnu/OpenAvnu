OPT=-O2
CFLAGS=$(OPT) -Wall -W -Wno-parentheses

CC=gcc
INCFLAGS=-I../../lib/igb -I../../daemons/mrpd -I../common
LDLIBS=-ligb -lpci -lz -lrt -lm -pthread
LDFLAGS=-L../../lib/igb

all: simple_talker

simple_talker: simple_talker.o ../common/talker_mrp_client.o

simple_talker.o: simple_talker.c
	gcc -c $(INCFLAGS) $(CFLAGS) simple_talker.c

../common/talker_mrp_client.o:
	make -C ../common/ talker_mrp_client.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` simple_talker
	rm -f ../common/talker_mrp_client.o

