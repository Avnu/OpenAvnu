OPT=-O2
#CFLAGS=$(OPT) -Wall -W -Wno-parentheses -Wstrict-prototypes -Wmissing-prototypes -ggdb
CFLAGS=$(OPT) -Wall -W -Wno-parentheses -ggdb

CC=gcc
#INCFLAGS=-I../lib
#LDLIBS=-ligb -lpci
#LDFLAGS=-L../lib

all: mrpd mrpctl

mrpd: mrpd.o

mrpctl: mrpctl.o

mrpctl.o: mrpctl.c
	gcc -c $(INCFLAGS) $(CFLAGS) mrpctl.c

mrpd.o: mrpd.c
	gcc -c $(INCFLAGS) $(CFLAGS) mrpd.c
%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	rm -f `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
	rm mrpd mrpctl

