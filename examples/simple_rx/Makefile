CC ?= gcc
OPT = -O2 -g
CFLAGS = $(OPT) -Wall -Wextra -Wno-parentheses
IGBDIR = ../../lib/igb
INCFLAGS = -I../../daemons/mrpd -I../common -I../../daemons/common -I$(IGBDIR)
# GLIBC versions starting with 2.17 don't need -lrt anymore
LDLIBS = -lpcap -lsndfile -pthread -lpci -lrt

all: simple_rx

simple_rx: simple_rx.o ../common/listener_mrp_client.o $(IGBDIR)/igb.o

simple_rx.o: simple_rx.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c simple_rx.c

../common/listener_mrp_client.o: ../common/listener_mrp_client.c ../common/listener_mrp_client.h
	make -C ../common/ listener_mrp_client.o

$(IGBDIR)/igb.o:  $(IGBDIR)/igb.c
	make -C $(IGBDIR) igb.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM)  simple_rx
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
