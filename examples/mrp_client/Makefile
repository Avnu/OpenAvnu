OPT=-O2
CFLAGS=$(OPT) -Wall -W -Wno-parentheses -ggdb

ifeq ($(ARCH),arm)
CC=arm-none-linux-gnueabi-gcc
else
CC=gcc
endif
#INCFLAGS=-I../../lib/igb
#LDLIBS=-ligb -lpci -lz -pthread
#LDFLAGS=-L../../lib/igb

all: mrpq mrpl

mrpl: mrpl.o mrpdclient.o

mrpq: mrpq.o mrpdclient.o

mrpl.o: mrpl.c
	$(CC) -c $(INCFLAGS) -I../../daemons/mrpd $(CFLAGS) mrpl.c

mrpq.o: mrpq.c
	$(CC) -c $(INCFLAGS) -I../../daemons/mrpd $(CFLAGS) mrpq.c

mrpdclient.o: mrpdclient.c
	$(CC) -c $(INCFLAGS) -I../../daemons/mrpd $(CFLAGS) mrpdclient.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"` mrpl mrpq

