CC?=gcc
OPT=-O2 -g
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses
INCFLAGS=-I../../lib/igb -I../../daemons/mrpd -I../common -I../../lib/common -I../../daemons/common
LDLIBS=-ligb -lpci -lrt -lm -pthread
LDFLAGS=-L../../lib/igb

all: simple_talker

simple_talker: simple_talker.o ../common/talker_mrp_client.o ../../lib/common/avb.o

simple_talker.o: simple_talker.c
	$(CC) $(CFLAGS) $(INCFLAGS) -c simple_talker.c

../../lib/common/avb.o: ../../lib/common/avb.h ../../lib/common/avb.c
	make -C ../../lib/common/ avb.o

../common/talker_mrp_client.o: ../common/talker_mrp_client.c ../common/talker_mrp_client.h
	make -C ../common/ talker_mrp_client.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

clean:
	$(RM) simple_talker
	$(RM) `find . -name "*~" -o -name "*.[oa]" -o -name "\#*\#" -o -name TAGS -o -name core -o -name "*.orig"`
