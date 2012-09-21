OPT=-O2
#CFLAGS=$(OPT) -Wall -W -Wno-parentheses -Wstrict-prototypes -Wmissing-prototypes -ggdb
CFLAGS=$(OPT) -Wall -W -Wno-parentheses -ggdb

CC=gcc
INCFLAGS=-I../lib
LDLIBS=-ligb -lpci -lz -pthread
LDFLAGS=-L../lib

all: test latency_test simple_talker mrpq mrpl

test: test.o

simple_talker: simple_talker.o

latency_test: latency_test.o

latency_test.o: latency_test.c
	gcc -c $(INCFLAGS) $(CFLAGS) latency_test.c

test.o: test.c
	gcc -c $(INCFLAGS) $(CFLAGS) test.c

simple_talker.o: simple_talker.c
	gcc -c $(INCFLAGS) -I../daemons/mrpd $(CFLAGS) simple_talker.c

mrpl: mrpl.o mrpdclient.o
	$(CC) mrpl.o mrpdclient.o -o $@

mrpq: mrpq.o mrpdclient.o
	$(CC) mrpq.o mrpdclient.o -o $@

%.o: %.c	
	gcc -c $(INCFLAGS) -I../daemons/mrpd $(CFLAGS) $^
	
%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
	
indent:
	indent --linux-style mrpdclient.c mrpdclient.h mrpl.c mrpq.c

