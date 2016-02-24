CC?=gcc
OPT=-O2 -g
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses
INCFLAGS=-I../../lib/igb -I../common 
LDLIBS=-ligb -lpci -lrt -lm -pthread
LDFLAGS=-L../../lib/igb

all: simple_rx

simple_rx: simple_rx.o

simple_rx.o: simple_rx.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c simple_rx.c

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) simple_rx
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
