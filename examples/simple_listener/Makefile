CC = gcc
OPT = -O2 -g
CFLAGS = $(OPT) -Wall -Wextra -Wno-parentheses
INCFLAGS = -I../../daemons/mrpd -I../common
LDLIBS = -lpcap -lsndfile

all: simple_listener

simple_listener: simple_listener.o ../common/listener_mrp_client.o

simple_listener.o: simple_listener.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c simple_listener.c

../common/listener_mrp_client.o: ../common/listener_mrp_client.c ../common/listener_mrp_client.h
	make -C ../common/ listener_mrp_client.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM)  simple_listener
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
