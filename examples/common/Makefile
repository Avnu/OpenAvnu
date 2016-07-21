CC ?= gcc
OPT = -O2 -g
CFLAGS = $(OPT) -Wall -Wextra -Wno-parentheses

all: talker_mrp_client.o listener_mrp_client.o

talker_mrp_client.o: talker_mrp_client.c talker_mrp_client.h
	$(CC) $(CFLAGS) -I../../daemons/mrpd  -I../../daemons/common -c talker_mrp_client.c

listener_mrp_client.o: listener_mrp_client.c listener_mrp_client.h
	$(CC) $(CFLAGS) -I../../daemons/mrpd  -I../../daemons/common -c listener_mrp_client.c

clean:
	$(RM)  talker_mrp_client.o listener_mrp_client.o
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
