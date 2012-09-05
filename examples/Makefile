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

mrpl: mrpl.o

mrpq: mrpq.o

latency_test: latency_test.o

latency_test.o: latency_test.c
	gcc -c $(INCFLAGS) $(CFLAGS) latency_test.c

test.o: test.c
	gcc -c $(INCFLAGS) $(CFLAGS) test.c

simple_talker.o: simple_talker.c
	gcc -c $(INCFLAGS) -I../daemons/mrpd $(CFLAGS) simple_talker.c

mrpl.o: mrpl.c
	gcc -c $(INCFLAGS) -I../daemons/mrpd $(CFLAGS) mrpl.c

mrpq.o: mrpq.c
	gcc -c $(INCFLAGS) -I../daemons/mrpd $(CFLAGS) mrpq.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`

