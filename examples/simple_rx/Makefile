CC?=gcc
OPT=-O2 -g
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses
IGBDIR=../../lib/igb
INCFLAGS=-I$(IGBDIR) -I../common
LDLIBS=-ligb -lpci -lrt -lm -pthread
LDFLAGS=-L$(IGBDIR)

all: simple_rx

simple_rx: $(IGBDIR)/igb.o simple_rx.o

simple_rx.o: simple_rx.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c simple_rx.c

igb.o:  $(IGBDIR)/igb.c
	make -C $(IGBDIR) igb.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) simple_rx
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
