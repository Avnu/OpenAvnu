CC = gcc
OPT = -O2 -g
CFLAGS = $(OPT) -Wall -Wextra -Wno-parentheses

all: avb.o talker_mrp_client.o listener_mrp_client.o

avb.o: avb.c avb.h
	$(CC) $(CFLAGS) -I../../lib/igb -c avb.c

talker_mrp_client.o: talker_mrp_client.c talker_mrp_client.h
	$(CC) $(CFLAGS) -I../../daemons/mrpd -c talker_mrp_client.c

listener_mrp_client.o: listener_mrp_client.c listener_mrp_client.h
	$(CC) $(CFLAGS) -I../../daemons/mrpd -c listener_mrp_client.c

clean:
	$(RM)  avb.o talker_mrp_client.o listener_mrp_client.o
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
