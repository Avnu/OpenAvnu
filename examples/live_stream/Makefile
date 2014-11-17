CFLAGS=$(OPT) -Wall -W -Wno-parentheses
EXTRA_FLAGS=-I ../../lib/igb/ -I../../daemons/mrpd -I../common
EXTRA_FLAGS+=-L ../../lib/igb/ -ligb -lpci -lrt -lpthread
INCFLAGS=../common/avb.c

all:talker \
	listener

talker: talker.o ../common/avb.o ../common/talker_mrp_client.o

talker.o: talker.c
	gcc -c $(INCFLAGS) talker.c  $(CFLAGS) $(EXTRA_FLAGS)

../common/avb.o:
	make -C ../common/ avb.o

../common/talker_mrp_client.o:
	make -C ../common/ talker_mrp_client.o

listener:listener.c
	gcc  -o listener $(INCFLAGS) listener.c $(CFLAGS) $(EXTRA_FLAGS)

%: %.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) $(EXTRA_FLAGS) -o $@

clean:
	rm -rf *.o *~ *.rej *.orig
	rm -rf talker listener
