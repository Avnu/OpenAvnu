CFLAGS=$(OPT) -Wall -W -Wno-parentheses
EXTRA_FLAGS=-I ../../lib/igb/ -I../../daemons/mrpd -I../common
EXTRA_FLAGS+=-L ../../lib/igb/ -ligb -lpci -lrt -lpthread
INCFLAGS=../common/avb.c

all:talker \
	listener

talker: talker.o ../common/avb.o ../common/talker_mrp_client.o

talker.o: talker.c
	gcc -c $(INCFLAGS) talker.c  $(CFLAGS) $(EXTRA_FLAGS)

../common/talker_mrp_client.o:
	make -C ../common/ talker_mrp_client.o

listener: listener.o ../common/avb.o ../common/listener_mrp_client.o

listener.o: listener.c
	gcc  -c $(INCFLAGS) listener.c $(CFLAGS) $(EXTRA_FLAGS)

../common/listener_mrp_client.o:
	make -C ../common/ listener_mrp_client.o

../common/avb.o:
	make -C ../common/ avb.o

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) $(EXTRA_FLAGS) -o $@

clean:
	rm -rf *.o *~ *.rej *.orig
	rm -rf talker listener
