CC=gcc
OPT=-O2 -g
CFLAGS=$(OPT) -Wall -Wextra -Wno-parentheses
INCFLAGS=-I ../../lib/igb/ -I../../daemons/mrpd -I../common
LDLIBS=-ligb -lpci -lrt -lpthread
LDFLAGS=-L ../../lib/igb/

all: talker listener

talker: talker.o ../common/avb.o ../common/talker_mrp_client.o

talker.o: talker.c
	$(CC) $(CFLAGS) $(INCFLAGS) $(EXTRA_FLAGS) -c talker.c

../common/talker_mrp_client.o: ../common/talker_mrp_client.c ../common/talker_mrp_client.h
	make -C ../common/ talker_mrp_client.o

listener: listener.o ../common/avb.o ../common/listener_mrp_client.o

listener.o: listener.c
	$(CC) $(CFLAGS) $(INCFLAGS) $(EXTRA_FLAGS) -c listener.c

../common/listener_mrp_client.o: ../common/listener_mrp_client.c ../common/listener_mrp_client.h
	make -C ../common/ listener_mrp_client.o

../common/avb.o: ../common/avb.c ../common/avb.h
	make -C ../common/ avb.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) $(EXTRA_FLAGS) -o $@

clean:
	$(RM) talker listener
	$(RM) *.o *~ *.rej *.orig
