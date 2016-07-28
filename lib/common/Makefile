CC ?= gcc
OPT = -O2 -g
CFLAGS = $(OPT) -Wall -Wextra -Wno-parentheses

all: avb.o

avb.o: avb.c avb.h
	$(CC) $(CFLAGS) -I../igb -c avb.c

clean:
	$(RM)  avb.o talker_mrp_client.o listener_mrp_client.o
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
